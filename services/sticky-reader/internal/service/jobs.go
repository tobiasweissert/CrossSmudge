package service

import (
	"context"
	"net/url"
	"os"
	"time"
)

func (s *Server) execute(w work) {
	j := w.job
	g := j.Group
	defer func() {
		g.cancel()
		s.mu.Lock()
		for _, member := range g.members {
			close(member.Done)
		}
		s.mu.Unlock()
	}()
	ctx, cancel := context.WithTimeout(w.ctx, 15*time.Second)
	defer cancel()
	finish := func(err error) {
		s.mu.Lock()
		defer s.mu.Unlock()
		for _, member := range g.members {
			if member.Err == "cancelled" {
				continue
			}
			if w.ctx.Err() != nil {
				member.Err = "cancelled"
			} else {
				member.Err = code(err)
			}
		}
	}
	if w.ctx.Err() != nil {
		finish(fail("cancelled"))
		return
	}
	s.mu.Lock()
	remaining := (64 << 20) - s.rates[j.Owner].Bytes
	s.mu.Unlock()
	ctx = boundedFetch(ctx, min(8<<20, remaining), 12<<20)
	f, e := s.cfg.Fetcher.Fetch(ctx, w.task.URL, "html")
	s.charge(j.Owner, f.Wire)
	if e != nil {
		finish(e)
		return
	}
	base, _ := url.Parse(f.URL)
	css := []string{}
	for _, u := range stylesheets(f.Data, base) {
		c, err := s.cfg.Fetcher.Fetch(ctx, u, "css")
		s.charge(j.Owner, c.Wire)
		f.Wire += c.Wire
		f.Decoded += c.Decoded
		if f.Wire > 8<<20 || f.Decoded > 12<<20 {
			finish(fail("too_large"))
			return
		}
		if err == nil {
			css = append(css, string(c.Data))
		}
	}
	r, e := runChild(ctx, Task{Kind: "html", Data: f.Data, URL: f.URL, Mode: w.task.Mode, CSS: css}, s.cfg.WorkerArgs, s.cfg.WorkerEnv)
	if e != nil {
		finish(e)
		return
	}
	if r.Document == nil {
		finish(fail("unreadable"))
		return
	}
	r.Document.Wire = f.Wire
	r.Document.Decoded = f.Decoded
	s.mu.Lock()
	quota := s.rates[j.Owner]
	over := quota.Bytes > 64<<20
	s.mu.Unlock()
	if over {
		finish(fail("busy"))
		return
	}
	if ctx.Err() != nil {
		finish(fail("timeout"))
		return
	}
	id, e := s.publish(j.Owner, j.Fingerprint, randomID(), r.Document)
	if e != nil {
		finish(e)
		return
	}
	s.mu.Lock()
	live := false
	for _, member := range g.members {
		if member.Err == "cancelled" {
			continue
		}
		if w.ctx.Err() != nil {
			member.Err = "cancelled"
		} else {
			member.Doc = id
			live = true
		}
	}
	if !live {
		s.removeDoc(id)
	}
	s.mu.Unlock()
}
func (s *Server) executeImage(w work) {
	ctx, cancel := context.WithTimeout(w.ctx, 15*time.Second)
	defer cancel()
	stop := context.AfterFunc(s.ctx, cancel)
	defer stop()
	done := func(b []byte, e error) {
		select {
		case w.result <- imageResult{b, e}:
		case <-w.ctx.Done():
		}
	}
	if ctx.Err() != nil {
		done(nil, fail("cancelled"))
		return
	}
	owner, docID := w.task.Mode, w.imageDoc
	doc, e := s.load(owner, docID)
	if e != nil {
		done(nil, e)
		return
	}
	source := doc.Assets[w.imageAsset]
	if source == "" {
		done(nil, fail("not_found"))
		return
	}
	key := w.imageAsset + ":" + strconvI(w.task.Width) + ":" + strconvI(w.task.Height)
	s.mu.Lock()
	en := s.docs[docID]
	cached := ""
	if en != nil {
		cached = en.Derivatives[key]
	}
	s.mu.Unlock()
	if cached != "" {
		b, e := os.ReadFile(cached)
		if e == nil {
			done(b, nil)
			return
		}
		// Never overwrite an indexed derivative whose physical state is uncertain.
		s.mu.Lock()
		s.removeDoc(docID)
		s.mu.Unlock()
		done(nil, fail("busy"))
		return
	}
	s.mu.Lock()
	en = s.docs[docID]
	if en == nil {
		s.mu.Unlock()
		done(nil, fail("expired"))
		return
	}
	if en.Removing || s.cacheUncertain() {
		s.mu.Unlock()
		done(nil, fail("busy"))
		return
	}
	remaining := min((64<<20)-s.rates[owner].Bytes, (8<<20)-en.Wire)
	remainingDecoded := (12 << 20) - en.Decoded
	s.mu.Unlock()
	ctx = boundedFetch(ctx, remaining, remainingDecoded)
	f, e := s.cfg.Fetcher.Fetch(ctx, source, "image")
	s.mu.Lock()
	en = s.docs[docID]
	if en == nil {
		s.mu.Unlock()
		done(nil, fail("expired"))
		return
	}
	en.Wire += f.Wire
	en.Decoded += f.Decoded
	s.rates[owner].Bytes += f.Wire
	over := en.Wire > 8<<20 || en.Decoded > 12<<20 || s.rates[owner].Bytes > 64<<20
	s.mu.Unlock()
	if over {
		done(nil, fail("too_large"))
		return
	}
	if e != nil {
		done(nil, e)
		return
	}
	r, e := runChild(ctx, Task{Kind: "image", Data: f.Data, Width: w.task.Width, Height: w.task.Height}, s.cfg.WorkerArgs, s.cfg.WorkerEnv)
	if e != nil {
		done(nil, e)
		return
	}
	if ctx.Err() != nil {
		done(nil, fail("cancelled"))
		return
	}
	s.mu.Lock()
	en = s.docs[docID]
	if en == nil || en.Removing || s.cacheUncertain() || len(en.Derivatives) >= 32 || s.disk+int64(len(r.PNG)) > 64<<20 {
		s.mu.Unlock()
		done(nil, fail("busy"))
		return
	}
	path := s.name(docID + key)
	e = s.writeCache(path, r.PNG)
	if e == nil {
		en.Derivatives[key] = path
		en.DerivativeSizes[key] = int64(len(r.PNG))
		s.disk += int64(len(r.PNG))
	}
	s.mu.Unlock()
	if e != nil {
		done(nil, fail("busy"))
		return
	}
	done(r.PNG, nil)
}

func (s *Server) charge(owner string, n int64) { s.mu.Lock(); s.rates[owner].Bytes += n; s.mu.Unlock() }
