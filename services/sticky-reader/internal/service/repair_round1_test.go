package service

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

func repairDoc(t *testing.T, input string) *Document {
	t.Helper()
	d, e := convertHTML([]byte(input), "https://example.com/", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	return d
}
func repairText(d *Document) string {
	var s strings.Builder
	for _, c := range d.Chunks {
		for _, b := range c.Blocks {
			for _, r := range b.Runs {
				s.WriteString(r.Text)
			}
		}
	}
	return s.String()
}
func TestRepairSEC002(t *testing.T) {
	s := newTestServer(t, nil)
	d := repairDoc(t, "<p>synthetic synthetic café</p>")
	id, e := s.publish("alice", "key", randomID(), d)
	if e != nil {
		t.Fatal(e)
	}
	for _, a := range []string{`{"chunk":null,"block":1,"run":1,"byte":1}`, `{"Chunk":0,"Block":1,"Run":1,"Byte":1}`, `{"chunk":0,"block":null,"run":1,"byte":1}`, `{"chunk":0,"block":1,"run":null,"byte":1}`, `{"chunk":0,"block":1,"run":1,"byte":null}`, `{"chunk":0,"block":1,"run":1}`, `{"chunk":0,"block":1,"run":1,"byte":1,"x":1}`, `{"chunk":0,"block":1,"run":1,"byte":1,"byte":2}`, `{"chunk":0.0,"block":1,"run":1,"byte":1}`, `{"chunk":1000000,"block":1,"run":1,"byte":1}`} {
		w := request(t, s, "/v1/find", fmt.Sprintf(`{"v":1,"doc":%q,"text":"synthetic","after":%s}`, id, a), "a")
		if w.Code != 400 {
			t.Errorf("accepted after %s: %d", a, w.Code)
		}
	}
	w := request(t, s, "/v1/find", fmt.Sprintf(`{"v":1,"doc":%q,"text":"synthetic","after":{"chunk":0,"block":1,"run":1,"byte":1}}`, id), "a")
	if w.Code != 200 || !strings.Contains(w.Body.String(), `"byte":11`) {
		t.Fatal(w.Code, w.Body.String())
	}
}
func TestRepairSEC003(t *testing.T) {
	for _, fields := range [][]string{{"identity", "br"}, {"gzip", "gzip"}, {"identity", "identity"}, {"identity, br"}, {"gzip, gzip"}, {"", "gzip"}} {
		t.Run(strings.Join(fields, "/"), func(t *testing.T) {
			f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				w.Header().Set("Content-Type", "text/html")
				for _, v := range fields {
					w.Header().Add("Content-Encoding", v)
				}
				io.WriteString(w, "<p>synthetic</p>")
			}))
			_, e := f.Fetch(context.Background(), "https://example.com/", "html")
			if code(e) != "unsupported_content" {
				t.Fatalf("encoding list %q accepted: %v", fields, e)
			}
		})
	}
}
func TestRepairMODEL001(t *testing.T) {
	for _, fragment := range []string{strings.Repeat("x", 250), strings.Repeat("&lt;", 250)} {
		for _, prefix := range []string{"", "<p>previous</p>"} {
			var input strings.Builder
			input.WriteString(prefix + "<p>")
			for i := 0; i < 64; i++ {
				fmt.Fprintf(&input, `<span id="%d%s">a</span>`, i, fragment)
			}
			input.WriteString("</p>")
			d, e := convertHTML([]byte(input.String()), "https://example.com/", "simplified")
			if e != nil {
				if code(e) != "too_large" {
					t.Fatal(e)
				}
				continue
			}
			for _, c := range d.Chunks {
				raw, _ := json.Marshal(c)
				if chunkText(c) > 8192 || len(raw) > 49152 || len(c.Anchors) > 64 {
					t.Errorf("invalid ready chunk decoded=%d serialized=%d anchors=%d", chunkText(c), len(raw), len(c.Anchors))
				}
			}
			if !strings.HasSuffix(repairText(d), strings.Repeat("a", 64)) {
				t.Error("retained text damaged")
			}
		}
	}
}
func TestRepairMODEL002(t *testing.T) {
	s := newTestServer(t, nil)
	d := repairDoc(t, "<p>private synthetic</p>")
	id, e := s.publish("alice", "key", randomID(), d)
	if e != nil {
		t.Fatal(e)
	}
	s.mu.Lock()
	en := s.docs[id]
	size := s.disk
	p := en.Path
	s.mu.Unlock()
	if e = os.Remove(p); e != nil {
		t.Fatal(e)
	}
	if e = os.Mkdir(p, 0700); e != nil {
		t.Fatal(e)
	}
	if e = os.WriteFile(filepath.Join(p, "block"), []byte("private"), 0600); e != nil {
		t.Fatal(e)
	}
	s.mu.Lock()
	s.removeDoc(id)
	owned := s.docs[id] != nil
	accounted := s.disk
	s.mu.Unlock()
	if !owned || accounted != size {
		t.Fatalf("failed deletion forgotten owned=%v disk=%d wanted=%d", owned, accounted, size)
	}
	if _, e = s.load("alice", id); e == nil {
		t.Fatal("quarantined object served")
	}
	if _, e = s.publish("alice", "other", randomID(), repairDoc(t, "<p>more</p>")); code(e) != "busy" {
		t.Fatal("admission after cleanup failure", e)
	}
	if e = os.Remove(filepath.Join(p, "block")); e != nil {
		t.Fatal(e)
	}
	s.mu.Lock()
	s.expire()
	owned = s.docs[id] != nil
	accounted = s.disk
	s.mu.Unlock()
	if owned || accounted != 0 {
		t.Fatal("cleanup retry did not release capacity", owned, accounted)
	}
}
func TestRepairMODEL003(t *testing.T) {
	for _, prefix := range []int{4093, 8189, 12285} {
		d := repairDoc(t, "<p>"+strings.Repeat("x", prefix)+"NEEDLEtail NEEDLE café</p><p>unrelated</p>")
		f := find(d, "needle", nil)
		if !f.Found {
			t.Errorf("continuation match lost at %d", prefix)
			continue
		}
		p := Position{*f.Chunk, f.Block, f.Run, f.Byte}
		if !validPosition(d, p) {
			t.Fatal("invalid cursor")
		}
		f = find(d, "NEEDLE", &p)
		if !f.Found {
			t.Fatal("exclusive next lost")
		}
		q := Position{*f.Chunk, f.Block, f.Run, f.Byte}
		if q == p {
			t.Fatal("not exclusive")
		}
		if find(d, "NEEDLE", &q).Found {
			t.Fatal("duplicate match")
		}
		if !find(d, "café", nil).Found || find(d, "CAFÉ", nil).Found {
			t.Fatal("nonASCII semantics")
		}
	}
	if find(repairDoc(t, "<p>NEE</p><p>DLE</p>"), "NEEDLE", nil).Found {
		t.Fatal("joined unrelated paragraphs")
	}
}
func TestRepairMODEL004(t *testing.T) {
	for _, tc := range []struct{ input, want string }{{`<p>hel<strong>lo</strong>, world</p>`, `hello, world`}, {`<p>one <strong>two</strong> three</p>`, `one two three`}, {`<p>one<strong> two </strong>three</p>`, `one two three`}, {`<p>café<a id="mark" href="/x">target</a>!</p>`, `cafétarget!`}, {"<pre> a\n  b\t c </pre>", " a\n  b\t c "}} {
		d := repairDoc(t, tc.input)
		if got := repairText(d); got != tc.want {
			t.Errorf("text %q want %q", got, tc.want)
		}
		if strings.Contains(tc.input, `id="mark"`) {
			a := d.Chunks[0].Anchors
			if len(a) != 1 {
				t.Fatal(a)
			}
			r := d.Chunks[0].Blocks[0].Runs[a[0].Run-1]
			if r.Text[a[0].Byte-1:] != "target" {
				t.Fatal("fragment not aligned", a, r)
			}
		}
	}
}

// The default stream mode exercises net/http byte behavior. Only the explicitly
// named TCP entrypoint below is kernel TCP evidence; it fails honestly on EPERM.
type repairCountConn struct {
	net.Conn
	n *atomic.Int64
}

func (c repairCountConn) Read(b []byte) (int, error) {
	n, e := c.Conn.Read(b)
	c.n.Add(int64(n))
	return n, e
}

type repairCountListener struct {
	net.Listener
	n atomic.Int64
}

func (l *repairCountListener) Accept() (net.Conn, error) {
	c, e := l.Listener.Accept()
	if e != nil {
		return nil, e
	}
	return repairCountConn{c, &l.n}, nil
}
func repairWire(t *testing.T, tcp bool) {
	for _, tc := range []struct {
		name, path, auth, framing, expect string
		status                            int
		rate                              bool
		payload                           string
	}{
		{name: "auth-length", path: "/v1/open", framing: "Content-Length: 1024\r\n", status: 401},
		{name: "auth-chunked", path: "/v1/open", framing: "Transfer-Encoding: chunked\r\n", status: 401},
		{name: "oversize", path: "/v1/open", auth: "a", framing: "Content-Length: 65536\r\n", status: 413},
		{name: "rate-chunked", path: "/v1/open", auth: "a", framing: "Transfer-Encoding: chunked\r\n", status: 429, rate: true},
		{name: "unsupported-chunked", path: "/absent", auth: "a", framing: "Transfer-Encoding: chunked\r\n", status: 404},
		{name: "oversize-expect", path: "/v1/open", auth: "a", framing: "Content-Length: 65536\r\n", expect: "Expect: 100-continue\r\n", status: 413},
		{name: "rate", path: "/v1/open", auth: "a", framing: "Content-Length: 1024\r\n", status: 429, rate: true},
		{name: "unsupported", path: "/absent", auth: "a", framing: "Content-Length: 1024\r\n", status: 404},
		{name: "expect", path: "/v1/open", framing: "Content-Length: 1024\r\n", expect: "Expect: 100-continue\r\n", status: 401},
		{name: "chunk-ceiling", path: "/v1/open", auth: "a", framing: "Transfer-Encoding: chunked\r\n", payload: "2000\r\n" + strings.Repeat("x", 2049), status: 413},
	} {
		t.Run(tc.name, func(t *testing.T) {
			s := newTestServer(t, nil)
			if tc.rate {
				s.mu.Lock()
				r := s.rates["alice"]
				r.Minute = s.cfg.Now()
				r.Ops = 60
				s.mu.Unlock()
			}
			var l net.Listener
			var dial func() (net.Conn, error)
			if tcp {
				var e error
				l, e = net.Listen("tcp", "127.0.0.1:0")
				if e != nil {
					t.Fatal("kernel TCP unavailable:", e)
				}
				dial = func() (net.Conn, error) { return net.DialTimeout("tcp", l.Addr().String(), time.Second) }
			} else {
				p := &pipeListener{conns: make(chan net.Conn), done: make(chan struct{})}
				l = p
				dial = func() (net.Conn, error) { return p.dial(context.Background(), "", "") }
			}
			counted := &repairCountListener{Listener: l}
			hs := &http.Server{Handler: s, ReadTimeout: 5 * time.Second, WriteTimeout: 20 * time.Second, MaxHeaderBytes: 8192}
			done := make(chan struct{})
			go func() { defer close(done); hs.Serve(LimitListener(counted)) }()
			defer func() { hs.Close(); <-done }()
			c, e := dial()
			if e != nil {
				t.Fatal(e)
			}
			defer c.Close()
			c.SetDeadline(time.Now().Add(time.Second))
			auth := ""
			if tc.auth != "" {
				auth = "Authorization: Bearer " + strings.Repeat(tc.auth, 64) + "\r\n"
			}
			headers := "POST " + tc.path + " HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n" + auth + tc.framing + tc.expect + "\r\n"
			_, e = io.WriteString(c, headers)
			if e != nil {
				t.Fatal(e)
			}
			if tc.payload != "" {
				if _, e = io.WriteString(c, tc.payload); e != nil {
					t.Fatal(e)
				}
			}
			resp, e := http.ReadResponse(bufio.NewReader(c), nil)
			if e != nil {
				t.Fatalf("response withheld pending prohibited body: %v", e)
			}
			defer resp.Body.Close()
			io.Copy(io.Discard, resp.Body)
			if resp.StatusCode != tc.status || !resp.Close {
				t.Fatalf("response status=%d close=%v", resp.StatusCode, resp.Close)
			}
			var end [1]byte
			if _, e = c.Read(end[:]); e != io.EOF {
				t.Fatalf("rejected connection still reading/reusable: %v", e)
			}
			bodyWire := counted.n.Load() - int64(len(headers))
			if bodyWire != int64(len(tc.payload)) {
				t.Fatalf("wire body bytes=%d want=%d (headers separated from payload)", bodyWire, len(tc.payload))
			}
			t.Logf("withheld-body response: status=%d HTTP body wire bytes=%d (payload ceiling 2048 + 1 overflow sentinel; chunk framing separate)", resp.StatusCode, bodyWire)

			// No body (or only 2049 bytes + chunk framing) was sent. A complete response
			// proves no drain of the withheld body; no OS read-ahead is counted as a drain.
		})
	}
}
func TestRepairSEC001Stream(t *testing.T) { repairWire(t, false) }
func TestRepairSEC001TCP(t *testing.T) {
	if os.Getenv("SRP_TCP_FIXTURES") != "1" {
		t.Skip("requires explicit kernel TCP qualification")
	}
	repairWire(t, true)
}

func TestRepairSEC001Reuse(t *testing.T) {
	s := newTestServer(t, nil)
	api := fixtureServer(t, s, false)
	defer api.Close()
	tr := api.Client().Transport.(*http.Transport)
	var calls int
	original := tr.DialContext
	tr.DialContext = func(ctx context.Context, n, a string) (net.Conn, error) { calls++; return original(ctx, n, a) }
	for i := 0; i < 2; i++ {
		status, _, _ := postWire(t, api.Client(), api.URL, "/v1/cancel", `{"v":1,"rid":"`+strings.Repeat("c", 32)+`"}`, "a")
		if status != 200 {
			t.Fatal(status)
		}
	}
	if calls != 1 {
		t.Fatal("fully consumed requests lost keepalive", calls)
	}
}

func repairOffset(t *testing.T, d *Document, p Position) int {
	t.Helper()
	offset := 0
	for ci, c := range d.Chunks {
		for _, b := range c.Blocks {
			for ri, r := range b.Runs {
				if ci == p.Chunk && b.ID == p.Block && ri+1 == p.Run {
					return offset + p.Byte - 1
				}
				offset += len(r.Text)
			}
		}
	}
	t.Fatal("cursor outside retained text")
	return -1
}
func TestRepairMODEL003ChunkAndUTF8(t *testing.T) {
	for _, tc := range []struct{ source, query string }{
		{strings.Repeat("x", 4093) + "NEEDLEtail " + strings.Repeat("x", 4080) + "NEEDLE" + strings.Repeat("x", 5000), "NEEDLE"},
		{strings.Repeat("x", 4095) + "éNEEDLE" + strings.Repeat("x", 10000), "xéN"},
	} {
		d := repairDoc(t, "<p>"+tc.source+"</p>")
		if len(d.Chunks) < 2 {
			t.Fatal("no actual chunk boundary")
		}
		f := find(d, tc.query, nil)
		if !f.Found {
			t.Fatal("chunk/UTF8 match lost")
		}
		p := Position{*f.Chunk, f.Block, f.Run, f.Byte}
		expected := strings.Index(tc.source, tc.query)
		if actual := repairOffset(t, d, p); actual != expected {
			t.Fatal("incorrect original start cursor", actual, expected)
		}
		// The match actually straddles chunks, rather than merely sharing a document.
		end := p.Byte - 1 + len(tc.query)
		block := d.Chunks[p.Chunk].Blocks[len(d.Chunks[p.Chunk].Blocks)-1]
		size := 0
		for _, r := range block.Runs {
			size += len(r.Text)
		}
		startInBlock := p.Byte - 1
		for i := 0; i < p.Run-1; i++ {
			startInBlock += len(block.Runs[i].Text)
		}
		if p.Block != block.ID || startInBlock+len(tc.query) <= size || end < 1 {
			t.Fatal("fixture did not cross chunk boundary")
		}
		next := find(d, tc.query, &p)
		rest := strings.Index(tc.source[expected+1:], tc.query)
		if rest < 0 {
			if next.Found {
				t.Fatal("invented next match")
			}
		} else {
			if !next.Found {
				t.Fatal("next match lost")
			}
			q := Position{*next.Chunk, next.Block, next.Run, next.Byte}
			if repairOffset(t, d, q) != expected+1+rest {
				t.Fatal("wrong next cursor")
			}
		}
	}
}
