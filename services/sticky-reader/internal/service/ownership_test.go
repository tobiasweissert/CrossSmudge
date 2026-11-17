package service

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"
)

func TestChildUncooperative(t *testing.T) {
	if os.Getenv("SRP_HANG_WORKER") == "1" {
		if path := os.Getenv("SRP_PID_MARK"); path != "" {
			os.WriteFile(path, []byte(strconv.Itoa(os.Getpid())), 0600)
		}
		for {
			time.Sleep(time.Hour)
		}
	}
	if os.Getenv("SRP_CRASH_WORKER") == "1" {
		os.Exit(7)
	}
}
func TestKillWaitAndCrash(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()
	start := time.Now()
	_, e := runChild(ctx, Task{}, []string{"-test.run=^TestChildUncooperative$"}, []string{"SRP_HANG_WORKER=1"})
	if code(e) != "timeout" || time.Since(start) > time.Second {
		t.Fatal("not killed and waited", e)
	}
	_, e = runChild(context.Background(), Task{}, []string{"-test.run=^TestChildUncooperative$"}, []string{"SRP_CRASH_WORKER=1"})
	if code(e) != "unreadable" {
		t.Fatal("crash adopted", e)
	}
}
func TestCachedRIDConflict(t *testing.T) {
	s := newTestServer(t, nil)
	d, _ := convertHTML([]byte(`<p>Original cache document with enough text.</p>`), "https://example.com/", "simplified")
	s.publish("alice", "https://example.com/\nsimplified\nsrp1-readeck2.1.3-fs1", randomID(), d)
	rid := strings.Repeat("3", 32)
	body := `{"v":1,"rid":"` + rid + `","url":"https://example.com/","mode":"simplified","images":"on-demand"}`
	if w := request(t, s, "/v1/open", body, "a"); w.Code != 200 {
		t.Fatal(w.Code)
	}
	if w := request(t, s, "/v1/open", strings.Replace(body, "example.com/", "example.com/different", 1), "a"); w.Code != 400 {
		t.Fatal("cached rid conflict lost", w.Code)
	}
}
func TestShutdownOwnsQueue(t *testing.T) {
	started := make(chan struct{})
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { close(started); <-r.Context().Done() }))
	s := newTestServer(t, f)
	for i, token := range []string{"a", "b"} {
		rid := strings.Repeat(string(rune('4'+i)), 32)
		w := request(t, s, "/v1/open", `{"v":1,"rid":"`+rid+`","url":"https://example.com/","mode":"reader","images":"on-demand"}`, token)
		if w.Code != 202 {
			t.Fatal(w.Code)
		}
		if i == 0 {
			<-started
		}
	}
	s.Close()
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, j := range s.jobs {
		select {
		case <-j.Done:
		default:
			t.Fatal("queue owner not relinquished")
		}
	}
}
func TestRequestTypesAndRate(t *testing.T) {
	s := newTestServer(t, nil)
	id := strings.Repeat("f", 32)
	for _, body := range []string{`{"v":1,"rid":"` + id + `","job":"` + id + `"}`, `{"v":1,"rid":null}`, `{"v":1.0,"rid":"` + id + `"}`, `{"v":1,"rid":"` + id + `","x":true}`, `{"v":1,"rid":"` + id + `"} {}`, `{"v":1,"rid":"` + id + `","rid":"` + id + `"}`} {
		if w := request(t, s, "/v1/cancel", body, "a"); w.Code != 400 {
			t.Fatal(w.Code, body)
		}
	}
	for i := 0; i < 70; i++ {
		w := request(t, s, "/v1/poll", `{"v":1,"job":"`+id+`"}`, "b")
		if w.Code != 404 {
			t.Fatal("poll allowance", w.Code)
		}
	}
	for i := 0; i < 60; i++ {
		request(t, s, "/v1/cancel", `{"v":1,"rid":"`+id+`"}`, "a")
	}
	if w := request(t, s, "/v1/open", `bad`, "a"); w.Code != 429 {
		t.Fatal("rate before body", w.Code)
	}
}
func TestSameURLSingleflightIndependentCancel(t *testing.T) {
	started := make(chan struct{})
	release := make(chan struct{})
	f, _, dials := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		close(started)
		select {
		case <-release:
		case <-r.Context().Done():
			return
		}
		w.Header().Set("Content-Type", "text/html")
		io.WriteString(w, `<p>Original shared work sufficient body.</p>`)
	}))
	s := newTestServer(t, f)
	open := func(rid string) *job {
		w := request(t, s, "/v1/open", `{"v":1,"rid":"`+rid+`","url":"https://example.com/","mode":"simplified","images":"on-demand"}`, "a")
		if w.Code != 202 {
			t.Fatalf("singleflight %d %s", w.Code, w.Body.String())
		}
		var p struct{ Job string }
		json.Unmarshal(w.Body.Bytes(), &p)
		s.mu.Lock()
		j := s.jobs[p.Job]
		s.mu.Unlock()
		return j
	}
	r1, r2 := strings.Repeat("7", 32), strings.Repeat("8", 32)
	open(r1)
	<-started
	j2 := open(r2)
	request(t, s, "/v1/cancel", `{"v":1,"rid":"`+r1+`"}`, "a")
	close(release)
	waitReady(t, s, j2.ID)
	if dials.Load() != 1 {
		t.Fatal("duplicate work")
	}
}

func TestQueueSaturationAndBoundedTombstones(t *testing.T) {
	started := make(chan struct{})
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		select {
		case <-started:
		default:
			close(started)
		}
		<-r.Context().Done()
	}))
	verifiers := map[string][32]byte{}
	for _, ch := range "abcdef" {
		verifiers[string(ch)] = sha256.Sum256([]byte(strings.Repeat(string(ch), 64)))
	}
	s, e := New(Config{CacheDir: t.TempDir(), Verifiers: verifiers, Fetcher: f})
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	for i, ch := range "abcdef" {
		w := request(t, s, "/v1/open", `{"v":1,"rid":"`+strings.Repeat(string(ch), 32)+`","url":"https://example.com/","mode":"reader","images":"on-demand"}`, string(ch))
		expected := 202
		if i == 5 {
			expected = 429
		}
		if w.Code != expected {
			t.Fatal(i, w.Code, w.Body.String())
		}
		if i == 0 {
			<-started
		}
	}
	s.mu.Lock()
	for i := 0; i < 256; i++ {
		s.tombs[fmt.Sprintf("a:%032x", i)] = time.Now().Add(time.Minute)
	}
	s.mu.Unlock()
	if w := request(t, s, "/v1/cancel", `{"v":1,"rid":"`+strings.Repeat("f", 32)+`"}`, "a"); w.Code != 429 {
		t.Fatal("tombstone capacity", w.Code)
	}
}
func TestCacheReset(t *testing.T) {
	dir := t.TempDir()
	if e := os.WriteFile(filepath.Join(dir, strings.Repeat("a", 64)), []byte("private"), 0600); e != nil {
		t.Fatal(e)
	}
	if e := ResetCache(dir); e != nil {
		t.Fatal(e)
	}
	files, _ := os.ReadDir(dir)
	if len(files) != 0 {
		t.Fatal("stale cache")
	}
	os.WriteFile(filepath.Join(dir, "unowned"), []byte("preserve"), 0600)
	if ResetCache(dir) == nil {
		t.Fatal("deleted unknown file")
	}
	if _, e := os.Stat(filepath.Join(dir, "unowned")); e != nil {
		t.Fatal("unknown removed")
	}
}

func TestAPICancelKillsAndReapsUncooperativeParser(t *testing.T) {
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		io.WriteString(w, `<p>Original sufficiently long parser fixture.</p>`)
	}))
	s := newTestServer(t, f)
	mark := filepath.Join(t.TempDir(), "child.pid")
	s.cfg.WorkerArgs = []string{"-test.run=^TestChildUncooperative$"}
	s.cfg.WorkerEnv = []string{"SRP_HANG_WORKER=1", "SRP_PID_MARK=" + mark}
	rid := strings.Repeat("6", 32)
	w := request(t, s, "/v1/open", `{"v":1,"rid":"`+rid+`","url":"https://example.com/","mode":"reader","images":"on-demand"}`, "a")
	if w.Code != 202 {
		t.Fatal(w.Code)
	}
	pid := 0
	for deadline := time.Now().Add(3 * time.Second); time.Now().Before(deadline); {
		raw, _ := os.ReadFile(mark)
		pid, _ = strconv.Atoi(string(raw))
		if pid > 0 {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}
	if pid == 0 {
		t.Fatal("child never entered")
	}
	s.mu.Lock()
	j := s.rid["alice:"+rid]
	s.mu.Unlock()
	w = request(t, s, "/v1/cancel", `{"v":1,"rid":"`+rid+`"}`, "a")
	if w.Code != 200 {
		t.Fatal(w.Code)
	}
	select {
	case <-j.Done:
	case <-time.After(2 * time.Second):
		t.Fatal("parser not reaped")
	}
	if e := syscall.Kill(pid, 0); e != syscall.ESRCH {
		t.Fatal("child alive or zombie after capacity release", pid, e)
	}
}

func TestHTTPConnectionAdmissionBound(t *testing.T) {
	base := &pipeListener{conns: make(chan net.Conn), done: make(chan struct{})}
	l := LimitListener(base)
	defer l.Close()
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	servers, clients := []net.Conn{}, []net.Conn{}
	defer func() {
		for _, c := range servers {
			c.Close()
		}
		for _, c := range clients {
			c.Close()
		}
	}()
	for i := 0; i < 16; i++ {
		client := make(chan net.Conn, 1)
		go func() { c, _ := base.dial(ctx, "", ""); client <- c }()
		c, e := l.Accept()
		if e != nil {
			t.Fatal(e)
		}
		servers = append(servers, c)
		clients = append(clients, <-client)
	}
	extra := make(chan net.Conn, 1)
	client := make(chan net.Conn, 1)
	go func() { c, _ := base.dial(ctx, "", ""); client <- c }()
	go func() { c, _ := l.Accept(); extra <- c }()
	select {
	case c := <-extra:
		if c != nil {
			c.Close()
		}
		t.Fatal("unbounded accepted connections")
	case <-time.After(30 * time.Millisecond):
	}
	servers[0].Close()
	select {
	case c := <-extra:
		if c == nil {
			t.Fatal("no connection after release")
		}
		servers = append(servers, c)
		clients = append(clients, <-client)
	case <-ctx.Done():
		t.Fatal("capacity not released")
	}
}
