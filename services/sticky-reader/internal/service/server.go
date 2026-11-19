package service

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/json"
	"errors"
	"io"
	"mime"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

type Config struct {
	CacheDir              string
	Verifiers             map[string][32]byte
	Fetcher               *Fetcher
	WorkerArgs, WorkerEnv []string
	Now                   func() time.Time
}
type entry struct {
	Owner, Path, Key string
	Expires, Used    time.Time
	Size             int64
	Wire, Decoded    int64
	Derivatives      map[string]string
	DerivativeSizes  map[string]int64
	Removing         bool
}
type jobGroup struct {
	cancel  context.CancelFunc
	members []*job
}
type job struct {
	Group                                 *jobGroup
	Owner, RID, ID, Fingerprint, Doc, Err string
	Created                               time.Time
	Cancel                                context.CancelFunc
	Done                                  chan struct{}
}
type rate struct {
	Minute            time.Time
	Ops, Polls, Opens int
	LastOpen          time.Time
	Day               time.Time
	Bytes             int64
}
type work struct {
	job                  *job
	ctx                  context.Context
	task                 Task
	imageDoc, imageAsset string
	result               chan imageResult
}
type imageResult struct {
	data []byte
	err  error
}
type Server struct {
	gone         map[string]time.Time
	cfg          Config
	mu           sync.Mutex
	ctx          context.Context
	cancel       context.CancelFunc
	wg           sync.WaitGroup
	queue        chan work
	httpSlots    chan struct{}
	jobs         map[string]*job
	rid          map[string]*job
	tombs        map[string]time.Time
	docs         map[string]*entry
	rates        map[string]*rate
	key          [32]byte
	disk         int64
	pending      map[string]int64
	cacheSyncDir func(string) error
	closed       bool
}

func New(c Config) (*Server, error) {
	if c.Verifiers != nil && (len(c.Verifiers) == 0 || len(c.Verifiers) > 16) {
		return nil, fail("unauthorized")
	}
	if c.Now == nil {
		c.Now = time.Now
	}
	if e := os.MkdirAll(c.CacheDir, 0700); e != nil {
		return nil, e
	}
	files, e := os.ReadDir(c.CacheDir)
	if e != nil || len(files) != 0 {
		return nil, fail("busy")
	}
	ctx, cancel := context.WithCancel(context.Background())
	s := &Server{cacheSyncDir: cacheDirSync, pending: map[string]int64{}, cfg: c, ctx: ctx, cancel: cancel, queue: make(chan work, 4), httpSlots: make(chan struct{}, 8), jobs: map[string]*job{}, rid: map[string]*job{}, tombs: map[string]time.Time{}, gone: map[string]time.Time{}, docs: map[string]*entry{}, rates: map[string]*rate{}}
	if _, e = rand.Read(s.key[:]); e != nil {
		cancel()
		return nil, e
	}
	for p := range c.Verifiers {
		s.rates[p] = &rate{}
	}
	if c.Verifiers == nil {
		s.rates["anonymous"] = &rate{}
	}
	s.wg.Add(1)
	go s.loop()
	return s, nil
}
func (s *Server) Close() {
	s.mu.Lock()
	if s.closed {
		s.mu.Unlock()
		return
	}
	s.closed = true
	s.cancel()
	for _, j := range s.jobs {
		j.Cancel()
	}
	s.mu.Unlock()
	s.wg.Wait()
}
func (s *Server) loop() {
	defer s.wg.Done()
	defer s.drainQueue()
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-s.ctx.Done():
			return
		case <-ticker.C:
			s.mu.Lock()
			s.expire()
			s.mu.Unlock()
		case w := <-s.queue:
			if w.job != nil {
				s.execute(w)
			} else {
				s.executeImage(w)
			}
		}
	}
}
func (s *Server) auth(r *http.Request) string {
	// Public mode has one bounded shared budget. No header, including legacy
	// Authorization or proxy identity, may allocate another principal.
	if s.cfg.Verifiers == nil {
		return "anonymous"
	}
	a := r.Header.Values("Authorization")
	if len(a) != 1 || !strings.HasPrefix(a[0], "Bearer ") {
		return ""
	}
	token := strings.TrimPrefix(a[0], "Bearer ")
	if !tokenPattern.MatchString(token) {
		return ""
	}
	hash := sha256.Sum256([]byte(token))
	owner := ""
	for id, v := range s.cfg.Verifiers {
		if subtle.ConstantTimeCompare(hash[:], v[:]) == 1 {
			owner = id
		}
	}
	return owner
}
func (s *Server) admit(owner, path string) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	r := s.rates[owner]
	now := s.cfg.Now()
	if now.Sub(r.Minute) >= time.Minute {
		r.Minute = now
		r.Ops = 0
		r.Polls = 0
		r.Opens = 0
	}
	if now.Sub(r.Day) >= 24*time.Hour {
		r.Day = now
		r.Bytes = 0
	}
	if path == "/v1/poll" {
		r.Polls++
		return r.Polls <= 120
	}
	r.Ops++
	return r.Ops <= 60 && r.Bytes < 64<<20
}
func writeJSON(w http.ResponseWriter, status int, v any) {
	b, e := json.Marshal(v)
	if e != nil || len(b) > 49152 {
		writeError(w, fail("too_large"))
		return
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Content-Length", strconvI(len(b)))
	w.WriteHeader(status)
	w.Write(b)
}
func writeError(w http.ResponseWriter, e error) {
	c := code(e)
	status := map[string]int{"invalid_request": 400, "unauthorized": 401, "forbidden": 403, "address_denied": 403, "tls_failed": 502, "upstream_denied": 502, "unsupported_content": 415, "too_large": 413, "unreadable": 422, "busy": 429, "timeout": 504, "cancelled": 410, "expired": 410, "not_found": 404}[c]
	if status == 0 {
		c = "unreadable"
		status = 422
	}
	writeJSON(w, status, struct {
		V         int    `json:"v"`
		Error     string `json:"error"`
		Retryable bool   `json:"retryable"`
	}{1, c, c == "busy" || c == "timeout"})
}
func (s *Server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	// Suppress net/http's pre-response drain. Until EOF is established, HTTP/1
	// replies close the connection and expire reads before the framework closes
	// the original body. Header/kernel read-ahead is accounted separately from intentional body reads.
	bodyDone := r.Body == nil || r.Body == http.NoBody
	if r.ProtoMajor == 1 && !bodyDone {
		if e := http.NewResponseController(w).EnableFullDuplex(); e != nil && !errors.Is(e, http.ErrNotSupported) {
			return
		}
		w.Header().Set("Connection", "close")
		defer func() {
			if !bodyDone {
				_ = http.NewResponseController(w).SetReadDeadline(time.Now())
				_ = r.Body.Close()
			}
		}()
	}

	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	if r.URL.Path == "/healthz" && r.Method == "GET" && r.URL.RawQuery == "" {
		writeJSON(w, 200, map[string]bool{"ok": true})
		return
	}
	s.mu.Lock()
	closed := s.closed
	s.mu.Unlock()
	if closed {
		writeError(w, fail("busy"))
		return
	}
	owner := s.auth(r)
	if owner == "" {
		writeError(w, fail("unauthorized"))
		return
	}
	headerBytes := len(r.Host) + len(r.RequestURI) + 16
	for k, vs := range r.Header {
		for _, v := range vs {
			headerBytes += len(k) + len(v) + 4
		}
	}
	if headerBytes > 8192 {
		writeError(w, fail("too_large"))
		return
	}
	if len(r.Header.Values("Origin")) > 1 || len(r.Header.Values("Content-Type")) > 1 || len(r.Header.Values("Content-Encoding")) > 1 {
		writeError(w, fail("invalid_request"))
		return
	}
	if !s.admit(owner, r.URL.Path) {
		writeError(w, fail("busy"))
		return
	}
	select {
	case s.httpSlots <- struct{}{}:
		defer func() { <-s.httpSlots }()
	default:
		writeError(w, fail("busy"))
		return
	}
	if r.Method != "POST" || r.URL.RawQuery != "" || r.URL.RawPath != "" || r.Header.Get("Origin") != "" && r.Header.Get("Origin") != Origin {
		writeError(w, fail("forbidden"))
		return
	}
	mt, params, e := mime.ParseMediaType(r.Header.Get("Content-Type"))
	if e != nil || mt != "application/json" || len(params) > 1 || params["charset"] != "" && !strings.EqualFold(params["charset"], "utf-8") || r.Header.Get("Content-Encoding") != "" && r.Header.Get("Content-Encoding") != "identity" {
		writeError(w, fail("invalid_request"))
		return
	}
	for k := range params {
		if k != "charset" {
			writeError(w, fail("invalid_request"))
			return
		}
	}
	if r.ContentLength > 2048 {
		writeError(w, fail("too_large"))
		return
	}
	shapes := map[string]string{"/v1/open": "v rid url mode images", "/v1/poll": "v job", "/v1/chunk": "v doc chunk", "/v1/locate": "v doc fragment", "/v1/find": "v doc text after", "/v1/image": "v doc asset width height", "/v1/cancel": "v rid"}
	shape, ok := shapes[r.URL.Path]
	if !ok {
		writeError(w, fail("not_found"))
		return
	}
	b, e := io.ReadAll(http.MaxBytesReader(w, r.Body, 2048))
	if e != nil || len(b) > 2048 {
		writeError(w, fail("too_large"))
		return
	}
	bodyDone = true
	w.Header().Del("Connection")
	var fields map[string]json.RawMessage
	if strictJSON(b, &fields) != nil {
		writeError(w, fail("invalid_request"))
		return
	}
	keys := strings.Fields(shape)
	if len(fields) != len(keys) {
		writeError(w, fail("invalid_request"))
		return
	}
	for _, k := range keys {
		if _, ok = fields[k]; !ok || string(fields[k]) == "null" && k != "after" {
			writeError(w, fail("invalid_request"))
			return
		}
	}
	var req struct {
		V        int       `json:"v"`
		RID      string    `json:"rid"`
		URL      string    `json:"url"`
		Mode     string    `json:"mode"`
		Images   string    `json:"images"`
		Job      string    `json:"job"`
		Doc      string    `json:"doc"`
		Chunk    int       `json:"chunk"`
		Fragment string    `json:"fragment"`
		Text     string    `json:"text"`
		After    *Position `json:"after"`
		Asset    string    `json:"asset"`
		Width    int       `json:"width"`
		Height   int       `json:"height"`
	}
	if strictJSON(b, &req) != nil || req.V != 1 {
		writeError(w, fail("invalid_request"))
		return
	}
	for _, k := range []string{"rid", "job", "doc", "asset"} {
		if raw, exists := fields[k]; exists {
			var id string
			if json.Unmarshal(raw, &id) != nil || !idPattern.MatchString(id) {
				writeError(w, fail("invalid_request"))
				return
			}
		}
	}
	switch r.URL.Path {
	case "/v1/open":
		if req.Mode != "reader" && req.Mode != "simplified" || req.Images != "on-demand" {
			writeError(w, fail("invalid_request"))
			return
		}
		u, e := safeURL(req.URL)
		if e != nil {
			writeError(w, e)
			return
		}
		s.open(w, owner, req.RID, u.String(), req.Mode)
	case "/v1/cancel":
		s.mu.Lock()
		s.expire()
		key := owner + ":" + req.RID
		if len(s.tombs) >= 256 {
			if _, ok := s.tombs[key]; !ok {
				s.mu.Unlock()
				writeError(w, fail("busy"))
				return
			}
		}
		s.tombs[key] = s.cfg.Now().Add(60 * time.Second)
		if j := s.rid[key]; j != nil {
			j.Cancel()
			j.Err = "cancelled"
		}
		s.mu.Unlock()
		writeJSON(w, 200, map[string]any{"v": 1, "state": "cancelled"})
	case "/v1/poll":
		s.mu.Lock()
		s.expire()
		j := s.jobs[req.Job]
		if j == nil || j.Owner != owner {
			s.mu.Unlock()
			writeError(w, fail("not_found"))
			return
		}
		doc, err := j.Doc, j.Err
		s.mu.Unlock()
		s.jobReply(w, owner, j.ID, doc, err)
	default:
		doc, e := s.load(owner, req.Doc)
		if e != nil {
			writeError(w, e)
			return
		}
		switch r.URL.Path {
		case "/v1/chunk":
			if req.Chunk < 0 || req.Chunk >= len(doc.Chunks) {
				writeError(w, fail("not_found"))
				return
			}
			writeJSON(w, 200, doc.Chunks[req.Chunk])
		case "/v1/locate":
			if len(req.Fragment) > 256 {
				writeError(w, fail("invalid_request"))
				return
			}
			for i, c := range doc.Chunks {
				for _, a := range c.Anchors {
					if a.ID == req.Fragment {
						writeJSON(w, 200, found(Position{i, a.Block, a.Run, a.Byte}))
						return
					}
				}
			}
			writeJSON(w, 200, Found{V: 1})
		case "/v1/find":
			if len(req.Text) == 0 || len(req.Text) > 100 || req.After != nil && !validPosition(doc, *req.After) {
				writeError(w, fail("invalid_request"))
				return
			}
			if _, ok := fields["after"]; !ok {
				writeError(w, fail("invalid_request"))
				return
			}
			if req.After != nil {
				var a map[string]json.RawMessage
				json.Unmarshal(fields["after"], &a)
				valid := len(a) == 4
				for _, k := range []string{"chunk", "block", "run", "byte"} {
					raw, present := a[k]
					var n int
					if !present || string(raw) == "null" || json.Unmarshal(raw, &n) != nil {
						valid = false
					}
				}
				if !valid {
					writeError(w, fail("invalid_request"))
					return
				}
			}
			writeJSON(w, 200, find(doc, req.Text, req.After))
		case "/v1/image":
			if req.Width < 1 || req.Height < 1 || req.Width > 800 || req.Height > 800 {
				writeError(w, fail("invalid_request"))
				return
			}
			if doc.Assets[req.Asset] == "" {
				writeError(w, fail("not_found"))
				return
			}
			s.mu.Lock()
			uncertain := s.cacheUncertain()
			s.mu.Unlock()
			if uncertain {
				writeError(w, fail("busy"))
				return
			}
			result := make(chan imageResult, 1)
			ctx, cancel := context.WithTimeout(r.Context(), 15*time.Second)
			defer cancel()
			work := work{ctx: ctx, imageDoc: req.Doc, imageAsset: req.Asset, task: Task{Mode: owner, Width: req.Width, Height: req.Height}, result: result}
			select {
			case s.queue <- work:
			default:
				writeError(w, fail("busy"))
				return
			}
			select {
			case out := <-result:
				if out.err != nil {
					writeError(w, out.err)
					return
				}
				w.Header().Set("Content-Type", "image/png")
				w.Header().Set("Content-Length", strconvI(len(out.data)))
				w.WriteHeader(200)
				w.Write(out.data)
			case <-ctx.Done():
				writeError(w, fail("timeout"))
			}
		}
	}
}
func (s *Server) open(w http.ResponseWriter, owner, rid, target, mode string) {
	fp := target + "\n" + mode + "\nsrp1-readeck2.1.3-fs1"
	s.mu.Lock()
	s.expire()
	if s.cacheUncertain() {
		s.mu.Unlock()
		writeError(w, fail("busy"))
		return
	}
	key := owner + ":" + rid
	if _, ok := s.tombs[key]; ok {
		s.mu.Unlock()
		writeError(w, fail("cancelled"))
		return
	}
	if j := s.rid[key]; j != nil {
		if j.Fingerprint != fp {
			s.mu.Unlock()
			writeError(w, fail("invalid_request"))
			return
		}
		id, doc, err := j.ID, j.Doc, j.Err
		s.mu.Unlock()
		s.jobReply(w, owner, id, doc, err)
		return
	}
	rate := s.rates[owner]
	now := s.cfg.Now()
	if rate.Opens >= 10 || rate.Opens >= 2 && now.Sub(rate.LastOpen) < 6*time.Second || len(s.jobs) >= 256 {
		s.mu.Unlock()
		writeError(w, fail("busy"))
		return
	}
	rate.Opens++
	rate.LastOpen = now
	for id, d := range s.docs {
		if d.Owner == owner && d.Key == fp {
			j := &job{Owner: owner, RID: rid, ID: randomID(), Fingerprint: fp, Created: now, Doc: id, Cancel: func() {}, Done: make(chan struct{})}
			close(j.Done)
			s.jobs[j.ID] = j
			s.rid[key] = j
			s.mu.Unlock()
			s.jobReply(w, owner, j.ID, id, "")
			return
		}
	}
	var shared *jobGroup
	for _, j := range s.jobs {
		if j.Owner == owner {
			select {
			case <-j.Done:
			default:
				if j.Fingerprint == fp && j.Err != "cancelled" {
					shared = j.Group
				} else {
					s.mu.Unlock()
					writeError(w, fail("busy"))
					return
				}
			}
		}
	}
	j := &job{Owner: owner, RID: rid, ID: randomID(), Fingerprint: fp, Created: now, Done: make(chan struct{})}
	if shared != nil {
		j.Group = shared
		shared.members = append(shared.members, j)
		s.setJobCancel(j)
		s.jobs[j.ID] = j
		s.rid[key] = j
		s.mu.Unlock()
		s.jobReply(w, owner, j.ID, "", "")
		return
	}
	ctx, cancel := context.WithTimeout(s.ctx, 60*time.Second)
	g := &jobGroup{cancel: cancel, members: []*job{j}}
	j.Group = g
	s.setJobCancel(j)
	select {
	case s.queue <- work{job: j, ctx: ctx, task: Task{URL: target, Mode: mode}}:
		s.jobs[j.ID] = j
		s.rid[key] = j
		s.mu.Unlock()
		s.jobReply(w, owner, j.ID, "", "")
	default:
		cancel()
		s.mu.Unlock()
		writeError(w, fail("busy"))
	}
}

// Called only with mu held, including the returned cancellation function.
func (s *Server) setJobCancel(j *job) {
	j.Cancel = func() {
		j.Err = "cancelled"
		for _, member := range j.Group.members {
			if member.Err != "cancelled" {
				return
			}
		}
		j.Group.cancel()
	}
}
func (s *Server) drainQueue() {
	s.mu.Lock()
	defer s.mu.Unlock()
	for {
		select {
		case w := <-s.queue:
			if w.job != nil {
				for _, j := range w.job.Group.members {
					j.Err = "cancelled"
					close(j.Done)
				}
			} else {
				select {
				case w.result <- imageResult{err: fail("cancelled")}:
				default:
				}
			}
		default:
			return
		}
	}
}

func (s *Server) jobReply(w http.ResponseWriter, owner, id, doc, err string) {
	if err != "" {
		writeError(w, fail(err))
		return
	}
	if doc != "" {
		d, e := s.load(owner, doc)
		if e != nil {
			writeError(w, e)
		} else {
			writeJSON(w, 200, d.Chunks[0])
		}
		return
	}
	writeJSON(w, 202, struct {
		V     int    `json:"v"`
		State string `json:"state"`
		Job   string `json:"job"`
		Retry int    `json:"retry_ms"`
	}{1, "pending", id, 500})
}
func validPosition(d *Document, p Position) bool {
	if p.Chunk < 0 || p.Chunk >= len(d.Chunks) || p.Run < 1 || p.Byte < 1 {
		return false
	}
	for _, b := range d.Chunks[p.Chunk].Blocks {
		if b.ID == p.Block && p.Run <= len(b.Runs) {
			t := b.Runs[p.Run-1].Text
			return p.Byte <= len(t)+1 && (p.Byte == len(t)+1 || utf8.RuneStart(t[p.Byte-1]))
		}
	}
	return false
}
func asciiFold(s string) string {
	b := []byte(s)
	for i, c := range b {
		if c >= 'A' && c <= 'Z' {
			b[i] += 32
		}
	}
	return string(b)
}
func find(d *Document, text string, after *Position) Found {
	needle := asciiFold(text)
	if len(needle) == 0 || len(needle) > 100 {
		return Found{V: 1}
	}
	// Fixed request-local overlap and cursors: at most the 100-byte query, even
	// across chunks. No whole-document concatenation or per-block heap churn.
	var failure [100]int
	var positions [100]Position
	for i, j := 1, 0; i < len(needle); i++ {
		for j > 0 && needle[i] != needle[j] {
			j = failure[j-1]
		}
		if needle[i] == needle[j] {
			j++
		}
		failure[i] = j
	}
	matched, seen, previousID := 0, 0, 0
	previousKind := ""
	for ci, c := range d.Chunks {
		for _, b := range c.Blocks {
			if !b.Continued || b.ID != previousID+1 || b.Kind != previousKind {
				matched = 0
				seen = 0
			}
			for ri, r := range b.Runs {
				for bi := 0; bi < len(r.Text); bi++ {
					ch := r.Text[bi]
					if ch >= 'A' && ch <= 'Z' {
						ch += 32
					}
					positions[seen%len(needle)] = Position{ci, b.ID, ri + 1, bi + 1}
					seen++
					for matched > 0 && ch != needle[matched] {
						matched = failure[matched-1]
					}
					if ch == needle[matched] {
						matched++
					}
					if matched == len(needle) {
						p := positions[(seen-len(needle))%len(needle)]
						if after == nil || p.Chunk > after.Chunk || p.Chunk == after.Chunk && (p.Block > after.Block || p.Block == after.Block && (p.Run > after.Run || p.Run == after.Run && p.Byte > after.Byte)) {
							return found(p)
						}
						matched = failure[matched-1]
					}
				}
			}
			previousID = b.ID
			previousKind = b.Kind
		}
	}
	return Found{V: 1}
}

func strconvI(n int) string { return strconv.Itoa(n) }
