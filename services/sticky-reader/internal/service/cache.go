package service

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"time"
)

func (s *Server) expire() {
	now := s.cfg.Now()
	for p, size := range s.pending {
		if s.removeCacheFile(p) == nil {
			delete(s.pending, p)
			s.disk -= size
		}
	}
	for id, d := range s.docs {
		if d.Removing || !now.Before(d.Expires) {
			s.removeDoc(id)
		}
	}
	for k, t := range s.gone {
		if !now.Before(t) {
			delete(s.gone, k)
		}
	}
	for k, t := range s.tombs {
		if !now.Before(t) {
			delete(s.tombs, k)
		}
	}
	for id, j := range s.jobs {
		select {
		case <-j.Done:
			if now.Sub(j.Created) > 60*time.Second {
				delete(s.jobs, id)
				delete(s.rid, j.Owner+":"+j.RID)
			}
		default:
		}
	}
}

// Failed removal retains the private object's index and capacity. Removing
// entries are unavailable to readers and block further physical publication.
func (s *Server) removeDoc(id string) bool {
	d := s.docs[id]
	if d == nil {
		return true
	}
	d.Removing = true
	failed := false
	for k, p := range d.Derivatives {
		if s.removeCacheFile(p) != nil {
			failed = true
			continue
		}
		s.disk -= d.DerivativeSizes[k]
		delete(d.DerivativeSizes, k)
		delete(d.Derivatives, k)
	}
	if d.Path != "" {
		if s.removeCacheFile(d.Path) != nil {
			failed = true
		} else {
			s.disk -= d.Size
			d.Size = 0
			d.Path = ""
		}
	}
	if failed {
		return false
	}
	if len(s.gone) >= 256 {
		old := ""
		var stamp time.Time
		for k, t := range s.gone {
			if old == "" || t.Before(stamp) {
				old = k
				stamp = t
			}
		}
		delete(s.gone, old)
	}
	s.gone[d.Owner+":"+id] = s.cfg.Now().Add(30 * time.Second)
	delete(s.docs, id)
	return true
}
func (s *Server) cacheUncertain() bool {
	if len(s.pending) > 0 {
		return true
	}
	for _, d := range s.docs {
		if d.Removing {
			return true
		}
	}
	return false
}

func cacheDirSync(path string) error {
	dir, e := os.Open(path)
	if e != nil {
		return e
	}
	defer dir.Close()
	return dir.Sync()
}

func (s *Server) removeCacheFile(path string) error {
	e := os.Remove(path)
	if e != nil && !os.IsNotExist(e) {
		return e
	}
	// A missing path may be the result of a previous unlink whose fsync failed.
	return s.cacheSyncDir(filepath.Dir(path))
}
func (s *Server) name(key string) string {
	h := hmac.New(sha256.New, s.key[:])
	h.Write([]byte(key))
	return filepath.Join(s.cfg.CacheDir, hex.EncodeToString(h.Sum(nil)))
}

// Called under mu after admission reserves space. Both temporary and renamed
// paths stay accounted until durability is confirmed, or confirmed cleanup.
func (s *Server) writeCache(path string, b []byte) error {
	f, e := os.CreateTemp(filepath.Dir(path), ".part-")
	if e != nil {
		return e
	}
	name := f.Name()
	size := int64(len(b))
	s.pending[name] = size
	s.disk += size
	if e = f.Chmod(0600); e == nil {
		_, e = f.Write(b)
	}
	if e == nil {
		e = f.Sync()
	}
	ce := f.Close()
	if e == nil {
		e = ce
	}
	if e == nil {
		e = os.Rename(name, path)
		if e == nil {
			delete(s.pending, name)
			name = path
			s.pending[name] = size
			e = s.cacheSyncDir(filepath.Dir(path))
		}
	}
	if e == nil {
		delete(s.pending, name)
		s.disk -= size
	}
	return e
}
func (s *Server) publish(owner, key, id string, d *Document) (string, error) {
	for i := range d.Chunks {
		d.Chunks[i].Doc = id
	}
	raw, e := json.Marshal(d)
	if e != nil || len(raw) > 1<<20 {
		return "", fail("too_large")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.expire()
	if s.cacheUncertain() || s.docs[id] != nil {
		return "", fail("busy")
	}
	counts := 0
	for _, d := range s.docs {
		if d.Owner == owner {
			counts++
		}
	}
	for len(s.docs) >= 64 || counts >= 16 || s.disk+int64(len(raw)) > 64<<20 {
		old := ""
		var stamp time.Time
		for k, v := range s.docs {
			if counts >= 16 && v.Owner != owner {
				continue
			}
			if old == "" || v.Used.Before(stamp) {
				old = k
				stamp = v.Used
			}
		}
		if old == "" {
			return "", fail("busy")
		}
		sameOwner := s.docs[old].Owner == owner
		if !s.removeDoc(old) {
			return "", fail("busy")
		}
		if sameOwner {
			counts--
		}
	}
	path := s.name(id)
	if e = s.writeCache(path, raw); e != nil {
		return "", fail("busy")
	}
	now := s.cfg.Now()
	s.docs[id] = &entry{Owner: owner, Path: path, Key: key, Expires: now.Add(30 * time.Minute), Used: now, Size: int64(len(raw)), Wire: d.Wire, Decoded: d.Decoded, Derivatives: map[string]string{}, DerivativeSizes: map[string]int64{}}
	s.disk += int64(len(raw))
	return id, nil
}
func (s *Server) load(owner, id string) (*Document, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.expire()
	d := s.docs[id]
	if d == nil || d.Owner != owner {
		if _, ok := s.gone[owner+":"+id]; ok {
			return nil, fail("expired")
		}
		return nil, fail("not_found")
	}
	if d.Removing {
		return nil, fail("expired")
	}
	d.Used = s.cfg.Now()
	raw, e := os.ReadFile(d.Path)
	if e != nil {
		return nil, fail("expired")
	}
	var doc Document
	if json.Unmarshal(raw, &doc) != nil {
		return nil, fail("expired")
	}
	return &doc, nil
}
