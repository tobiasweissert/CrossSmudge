package service

import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"net/netip"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

type fakeDNS struct {
	answers []string
	calls   atomic.Int32
}

func (f *fakeDNS) LookupNetIP(ctx context.Context, network, h string) ([]netip.Addr, error) {
	f.calls.Add(1)
	out := []netip.Addr{}
	for _, a := range f.answers {
		out = append(out, netip.MustParseAddr(a))
	}
	return out, nil
}

type fakePeer struct {
	net.Conn
	peer string
}

func (c fakePeer) RemoteAddr() net.Addr { return &net.TCPAddr{IP: net.ParseIP(c.peer), Port: 443} }

// Only DNS and the numeric-to-loopback dial are fake boundaries. TLS, HTTP, gzip, extraction and codecs are real.
func fixtureFetcher(t *testing.T, h http.Handler, tlsConfigs ...*tls.Config) (*Fetcher, *fakeDNS, *atomic.Int32) {
	t.Helper()
	ts := fixtureServer(t, h, true, tlsConfigs...)
	t.Cleanup(ts.Close)
	roots := x509.NewCertPool()
	roots.AddCert(ts.Certificate())
	dns := &fakeDNS{answers: []string{"93.184.216.34"}}
	dials := &atomic.Int32{}
	return &Fetcher{Resolver: dns, Roots: roots, Dial: func(ctx context.Context, network, addr string) (net.Conn, error) {
		dials.Add(1)
		if addr != "93.184.216.34:443" {
			return nil, fmt.Errorf("pin missing")
		}
		c, e := ts.Client().Transport.(*http.Transport).DialContext(ctx, "tcp", ts.Listener.Addr().String())
		if e != nil {
			return nil, e
		}
		return fakePeer{c, "93.184.216.34"}, nil
	}}, dns, dials
}
func postWire(t *testing.T, client *http.Client, origin, path, body, token string) (int, []byte, http.Header) {
	t.Helper()
	r, _ := http.NewRequest("POST", origin+path, strings.NewReader(body))
	r.Header.Set("Content-Type", "application/json")
	r.Header.Set("Authorization", "Bearer "+strings.Repeat(token, 64))
	resp, e := client.Do(r)
	if e != nil {
		t.Fatal(e)
	}
	defer resp.Body.Close()
	b, e := io.ReadAll(resp.Body)
	if e != nil {
		t.Fatal(e)
	}
	return resp.StatusCode, b, resp.Header
}
func waitReady(t *testing.T, s *Server, job string) Chunk {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		w := request(t, s, "/v1/poll", `{"v":1,"job":"`+job+`"}`, "a")
		if w.Code == 200 {
			var c Chunk
			if e := json.Unmarshal(w.Body.Bytes(), &c); e != nil {
				t.Fatal(e)
			}
			return c
		}
		if w.Code != 202 {
			t.Fatalf("poll %d %s", w.Code, w.Body.String())
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal("job timed out")
	return Chunk{}
}
func TestActualHTTPFlow(t *testing.T) {
	im := image.NewNRGBA(image.Rect(0, 0, 120, 60))
	for y := 0; y < 60; y++ {
		for x := 0; x < 120; x++ {
			im.Set(x, y, color.NRGBA{128, 128, 128, 255})
		}
	}
	var imageBytes bytes.Buffer
	png.Encode(&imageBytes, im)
	var upstream atomic.Int32
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		upstream.Add(1)
		if r.Header.Get("Authorization") != "" || r.Header.Get("Cookie") != "" || r.Header.Get("Referer") != "" {
			t.Error("credential forwarding")
		}
		switch r.URL.Path {
		case "/photo.png":
			w.Header().Set("Content-Type", "image/png")
			w.Write(imageBytes.Bytes())
		case "/style.css":
			w.Header().Set("Content-Type", "text/css")
			io.WriteString(w, ".concealed {display:none}")
		default:
			w.Header().Set("Content-Type", "text/html")
			io.WriteString(w, `<html lang="en"><title>Original fixture</title><link rel="stylesheet" href="/style.css"><body><h1 id="top">Heading</h1><p>`+strings.Repeat("Actual HTTP UTF8 café content. ", 900)+`</p><p id="end">TAIL_SECRET_SYNTHETIC <a href="/next">Next</a></p><p class="concealed">CSS_HIDDEN</p><img src="/photo.png" width="120" height="60" alt="Synthetic midtone"><form action="/search"><input type="search" name="q"></form></body></html>`)
		}
	}))
	s := newTestServer(t, f)
	api := fixtureServer(t, s, false)
	t.Cleanup(api.Close)
	client := api.Client()
	rid := strings.Repeat("1", 32)
	body := `{"v":1,"rid":"` + rid + `","url":"https://example.com/story","mode":"simplified","images":"on-demand"}`
	status, raw, _ := postWire(t, client, api.URL, "/v1/open", body, "a")
	if status != 202 {
		t.Fatalf("open %d %s", status, raw)
	}
	var pending struct {
		Job string `json:"job"`
	}
	json.Unmarshal(raw, &pending)
	c := waitReady(t, s, pending.Job)
	if c.Chunks < 2 {
		t.Fatal("not chunked")
	}
	var asset string
	tail := false
	for i := 0; i < c.Chunks; i++ {
		status, b, _ := postWire(t, client, api.URL, "/v1/chunk", fmt.Sprintf(`{"v":1,"doc":%q,"chunk":%d}`, c.Doc, i), "a")
		if status != 200 || len(b) > 49152 {
			t.Fatal(status)
		}
		if bytes.Contains(b, []byte("TAIL_SECRET_SYNTHETIC")) {
			tail = true
		}
		if bytes.Contains(b, []byte("CSS_HIDDEN")) {
			t.Fatal("external CSS ignored")
		}
		var chunk Chunk
		json.Unmarshal(b, &chunk)
		for _, block := range chunk.Blocks {
			if block.Image != nil {
				asset = block.Image.Asset
			}
		}
	}
	if !tail || asset == "" {
		t.Fatal("tail/image lost")
	}
	status, b, h := postWire(t, client, api.URL, "/v1/image", fmt.Sprintf(`{"v":1,"doc":%q,"asset":%q,"width":80,"height":80}`, c.Doc, asset), "a")
	if status != 200 || h.Get("Content-Type") != "image/png" || b[24] != 1 || b[25] != 3 {
		t.Fatalf("image %d %.100s", status, b)
	}
	if h.Get("Content-Length") != fmt.Sprint(len(b)) {
		t.Fatal("length")
	}
	for _, pathBody := range [][2]string{{"/v1/chunk", fmt.Sprintf(`{"v":1,"doc":%q,"chunk":0}`, c.Doc)}, {"/v1/poll", fmt.Sprintf(`{"v":1,"job":%q}`, pending.Job)}, {"/v1/image", fmt.Sprintf(`{"v":1,"doc":%q,"asset":%q,"width":80,"height":80}`, c.Doc, asset)}} {
		status, _, _ = postWire(t, client, api.URL, pathBody[0], pathBody[1], "b")
		if status != 404 {
			t.Fatalf("cross principal %s %d", pathBody[0], status)
		}
	}
	status, b, _ = postWire(t, client, api.URL, "/v1/locate", fmt.Sprintf(`{"v":1,"doc":%q,"fragment":"end"}`, c.Doc), "a")
	if status != 200 || !bytes.Contains(b, []byte(`"found":true`)) {
		t.Fatal(status, string(b))
	}
	status, b, _ = postWire(t, client, api.URL, "/v1/find", fmt.Sprintf(`{"v":1,"doc":%q,"text":"tail_secret","after":null}`, c.Doc), "a")
	if status != 200 || !bytes.Contains(b, []byte(`"found":true`)) {
		t.Fatal(status, string(b))
	}
	before := upstream.Load()
	status, _, _ = postWire(t, client, api.URL, "/v1/open", body, "a")
	if status != 200 || upstream.Load() != before {
		t.Fatal("idempotency")
	}
}
func TestFetchSecurityAndGzip(t *testing.T) {
	var seen atomic.Int32
	f, dns, dials := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		seen.Add(1)
		if r.URL.Path == "/redirect" {
			w.Header().Set("Location", "https://127.0.0.1/")
			w.WriteHeader(302)
			return
		}
		w.Header().Set("Content-Type", "text/html")
		io.WriteString(w, "<p>safe</p>")
	}))
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*3)
	defer cancel()
	if _, e := f.Fetch(ctx, "https://example.com/", "html"); e != nil {
		t.Fatal(e)
	}
	old := dials.Load()
	if _, e := f.Fetch(ctx, "https://example.com/redirect", "html"); code(e) != "address_denied" || dials.Load() != old+1 {
		t.Fatalf("redirect policy %v", e)
	}
	dns.answers = []string{"93.184.216.34", "10.0.0.1"}
	old = dials.Load()
	if _, e := f.Fetch(ctx, "https://example.com/", "html"); code(e) != "address_denied" || dials.Load() != old {
		t.Fatal("mixed DNS dialed")
	}
	var b bytes.Buffer
	g := gzip.NewWriter(&b)
	g.Write([]byte("original gzip"))
	g.Close()
	good := append([]byte(nil), b.Bytes()...)
	out, e := decodeBody(good, "gzip", 1024)
	if e != nil || string(out) != "original gzip" {
		t.Fatal(e)
	}
	corrupt := append([]byte(nil), good...)
	corrupt[len(corrupt)-5] ^= 1
	for _, raw := range [][]byte{corrupt, append(append([]byte(nil), good...), good...), append(append([]byte(nil), good...), 1), good[:len(good)-2]} {
		if _, e := decodeBody(raw, "gzip", 1024); e == nil {
			t.Fatal("accepted broken/member gzip")
		}
	}
	b.Reset()
	g = gzip.NewWriter(&b)
	g.Write(bytes.Repeat([]byte("a"), 100000))
	g.Close()
	if _, e := decodeBody(b.Bytes(), "gzip", 4<<20); code(e) != "too_large" {
		t.Fatal("gzip bomb")
	}
	f.Roots = x509.NewCertPool()
	dns.answers = []string{"93.184.216.34"}
	if _, e := f.Fetch(ctx, "https://example.com/", "html"); code(e) != "tls_failed" {
		t.Fatal("untrusted TLS", e)
	}
}
func TestCancelFetchAndCapacity(t *testing.T) {
	started := make(chan struct{}, 4)
	drained := make(chan struct{}, 4)
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		started <- struct{}{}
		<-r.Context().Done()
		drained <- struct{}{}
	}))
	s := newTestServer(t, f)
	rid := strings.Repeat("2", 32)
	body := `{"v":1,"rid":"` + rid + `","url":"https://example.com/blocked","mode":"reader","images":"on-demand"}`
	w := request(t, s, "/v1/open", body, "a")
	if w.Code != 202 {
		t.Fatal(w.Code)
	}
	select {
	case <-started:
	case <-time.After(time.Second):
		t.Fatal("fetch not started")
	}
	w = request(t, s, "/v1/cancel", `{"v":1,"rid":"`+rid+`"}`, "a")
	if w.Code != 200 {
		t.Fatal(w.Code)
	}
	select {
	case <-drained:
	case <-time.After(time.Second):
		t.Fatal("fetch not drained")
	}
	s.mu.Lock()
	j := s.rid["alice:"+rid]
	s.mu.Unlock()
	select {
	case <-j.Done:
	case <-time.After(time.Second):
		t.Fatal("job not released")
	}
}
func TestConcurrentCacheExpiryAndBounds(t *testing.T) {
	s := newTestServer(t, nil)
	d, e := convertHTML([]byte(`<h1>Original</h1><p>Private synthetic cache body only.</p>`), "https://example.com/", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	var wg sync.WaitGroup
	for i := 0; i < 20; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			copyDoc := *d
			copyDoc.Chunks = append([]Chunk(nil), d.Chunks...)
			id, e := s.publish("alice", fmt.Sprint(i), randomID(), &copyDoc)
			if e != nil {
				t.Error(e)
				return
			}
			s.load("bob", id)
		}(i)
	}
	wg.Wait()
	s.mu.Lock()
	if len(s.docs) > 16 || s.disk > 64<<20 {
		t.Fatal("cache bounds")
	}
	for _, en := range s.docs {
		en.Expires = time.Now().Add(-time.Second)
	}
	s.expire()
	n := len(s.docs)
	disk := s.disk
	s.mu.Unlock()
	if n != 0 || disk != 0 {
		t.Fatal(n, disk)
	}
	files, _ := os.ReadDir(s.cfg.CacheDir)
	if len(files) != 0 {
		t.Fatal("disk leak")
	}
}
func TestAnchorExactOffset(t *testing.T) {
	d, e := convertHTML([]byte(`<p>Prefix <span id="mid">café target</span> suffix.</p>`), "https://example.com/", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	for _, a := range d.Chunks[0].Anchors {
		if a.ID == "mid" {
			if a.Byte != 8 {
				t.Fatalf("invented anchor byte %d", a.Byte)
			}
			return
		}
	}
	t.Fatal("anchor missing")
}

// In-process stream fixture runs real HTTP/TLS bytes with explicitly fake connection boundaries.
// SRP_TCP_FIXTURES=1 runs the same assertions over actual loopback TCP (required before deployment).
type pipeListener struct {
	conns chan net.Conn
	done  chan struct{}
	once  sync.Once
}

func (l *pipeListener) Accept() (net.Conn, error) {
	select {
	case c := <-l.conns:
		return c, nil
	case <-l.done:
		return nil, net.ErrClosed
	}
}
func (l *pipeListener) Close() error   { l.once.Do(func() { close(l.done) }); return nil }
func (l *pipeListener) Addr() net.Addr { return &net.TCPAddr{IP: net.IPv4(127, 0, 0, 1), Port: 1} }
func (l *pipeListener) dial(ctx context.Context, network, addr string) (net.Conn, error) {
	a, b := net.Pipe()
	select {
	case l.conns <- a:
		return b, nil
	case <-ctx.Done():
		a.Close()
		b.Close()
		return nil, ctx.Err()
	case <-l.done:
		a.Close()
		b.Close()
		return nil, net.ErrClosed
	}
}
func fixtureServer(t *testing.T, h http.Handler, tlsMode bool, tlsConfigs ...*tls.Config) *httptest.Server {
	t.Helper()
	if os.Getenv("SRP_TCP_FIXTURES") == "1" {
		var ts *httptest.Server
		if tlsMode {
			ts = httptest.NewUnstartedServer(h)
			if len(tlsConfigs) > 0 {
				ts.TLS = tlsConfigs[0]
			}
			ts.StartTLS()
		} else {
			ts = httptest.NewServer(h)
		}
		tr := ts.Client().Transport.(*http.Transport)
		tr.DialContext = (&net.Dialer{}).DialContext
		return ts
	}
	listener := &pipeListener{conns: make(chan net.Conn), done: make(chan struct{})}
	ts := &httptest.Server{Listener: listener, Config: &http.Server{Handler: h}}
	if tlsMode {
		if len(tlsConfigs) > 0 {
			ts.TLS = tlsConfigs[0]
		}
		ts.StartTLS()
	} else {
		ts.Start()
	}
	ts.Client().Transport.(*http.Transport).DialContext = listener.dial
	return ts
}
