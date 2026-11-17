package service

import (
	"bytes"
	"context"
	"fmt"
	"image"
	"image/png"
	"net/http"
	"os"
	"path/filepath"
	"testing"
	"time"
)

// Test seam changes only directory durability outcome, after the real rename.
// The per-server seam is changed under its ownership mutex.
func TestRepairMODEL002PostRename(t *testing.T) {
	s := newTestServer(t, nil)
	original := s.cacheSyncDir
	s.mu.Lock()
	s.cacheSyncDir = func(string) error { return os.ErrPermission }
	s.mu.Unlock()
	defer func() { s.mu.Lock(); s.cacheSyncDir = original; s.mu.Unlock() }()
	d := repairDoc(t, "<p>private synthetic uncertain publication</p>")
	if _, e := s.publish("alice", "key", randomID(), d); code(e) != "busy" {
		t.Fatal("uncertain publication accepted", e)
	}
	files, e := os.ReadDir(s.cfg.CacheDir)
	if e != nil || len(files) != 1 {
		t.Fatal("real renamed object absent", e, len(files))
	}
	s.mu.Lock()
	disk := s.disk
	s.mu.Unlock()
	if disk <= 0 {
		t.Fatal("renamed private object unaccounted")
	}
	if _, e = s.publish("alice", "other", randomID(), repairDoc(t, "<p>more</p>")); code(e) != "busy" {
		t.Fatal("uncertain cleanup admitted growth", e)
	}
	s.mu.Lock()
	s.cacheSyncDir = original
	s.mu.Unlock()
	s.mu.Lock()
	s.expire()
	disk = s.disk
	s.mu.Unlock()
	files, e = os.ReadDir(s.cfg.CacheDir)
	if e != nil || len(files) != 0 || disk != 0 {
		t.Fatal("retry failed", e, len(files), disk)
	}
	if _, e = s.publish("alice", "recovered", randomID(), repairDoc(t, "<p>recovered</p>")); e != nil {
		t.Fatal("admission did not recover", e)
	}
}

func TestRepairMODEL002TTLRestartAndCapacity(t *testing.T) {
	s := newTestServer(t, nil)
	d := repairDoc(t, "<p>private synthetic ttl</p>")
	id, e := s.publish("alice", "key", randomID(), d)
	if e != nil {
		t.Fatal(e)
	}
	if other, e := New(Config{CacheDir: s.cfg.CacheDir, Verifiers: s.cfg.Verifiers}); e == nil {
		other.Close()
		t.Fatal("restart admitted occupied cache")
	}
	s.mu.Lock()
	s.docs[id].Expires = s.cfg.Now().Add(-time.Second)
	s.cacheSyncDir = func(string) error { return os.ErrPermission }
	s.expire()
	disk := s.disk
	en := s.docs[id]
	s.mu.Unlock()
	if disk == 0 || en == nil || !en.Removing {
		t.Fatal("TTL durability failure forgotten")
	}

	// ResetCache only removes known regular cache objects; unknown files survive.
	unknown := filepath.Join(s.cfg.CacheDir, "operator-file")
	if e = os.WriteFile(unknown, []byte("preserve"), 0600); e != nil {
		t.Fatal(e)
	}
	if ResetCache(s.cfg.CacheDir) == nil {
		t.Fatal("reset discarded unrelated file")
	}
	if _, e = os.Stat(unknown); e != nil {
		t.Fatal(e)
	}
	if e = os.Remove(unknown); e != nil {
		t.Fatal(e)
	}
	s.mu.Lock()
	s.cacheSyncDir = cacheDirSync
	s.expire()
	disk = s.disk
	s.mu.Unlock()
	if disk != 0 {
		t.Fatal("TTL retry not accounted", disk)
	}
	if e = ResetCache(s.cfg.CacheDir); e != nil {
		t.Fatal(e)
	}
	restarted, e := New(Config{CacheDir: s.cfg.CacheDir, Verifiers: s.cfg.Verifiers})
	if e != nil {
		t.Fatal(e)
	}
	restarted.Close()
	// Exercise eviction under a real principal entry limit, with deletion failure.
	for i := 0; i < 16; i++ {
		if _, e = s.publish("alice", fmt.Sprint(i), randomID(), repairDoc(t, "<p>bounded</p>")); e != nil {
			t.Fatal(e)
		}
	}
	s.mu.Lock()
	s.cacheSyncDir = func(string) error { return os.ErrPermission }
	before := s.disk
	s.mu.Unlock()
	if _, e = s.publish("alice", "overflow", randomID(), repairDoc(t, "<p>blocked</p>")); code(e) != "busy" {
		t.Fatal("eviction uncertainty accepted", e)
	}
	s.mu.Lock()
	count := len(s.docs)
	after := s.disk
	s.cacheSyncDir = cacheDirSync
	s.expire()
	s.mu.Unlock()
	if count != 16 || after != before {
		t.Fatal("eviction released uncertain capacity", count, after, before)
	}
}

func TestRepairMODEL002ImagePostRename(t *testing.T) {
	var imageBytes bytes.Buffer
	if e := png.Encode(&imageBytes, image.NewGray(image.Rect(0, 0, 32, 32))); e != nil {
		t.Fatal(e)
	}
	f, _, dials := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "image/png")
		w.Write(imageBytes.Bytes())
	}))
	s := newTestServer(t, f)
	d := repairDoc(t, `<p>private synthetic</p><img src="/image.png" width="32" height="32">`)
	id, e := s.publish("alice", "key", randomID(), d)
	if e != nil {
		t.Fatal(e)
	}
	asset := ""
	for a := range d.Assets {
		asset = a
	}
	s.mu.Lock()
	before := s.disk
	s.cacheSyncDir = func(string) error { return os.ErrPermission }
	s.mu.Unlock()
	result := make(chan imageResult, 1)
	s.executeImage(work{ctx: context.Background(), imageDoc: id, imageAsset: asset, task: Task{Mode: "alice", Width: 32, Height: 32}, result: result})
	if out := <-result; code(out.err) != "busy" {
		t.Fatal("uncertain image served", out.err)
	}
	s.mu.Lock()
	after := s.disk
	pending := len(s.pending)
	s.mu.Unlock()
	if after <= before || pending != 1 {
		t.Fatal("image destination unaccounted", before, after, pending)
	}
	files, e := os.ReadDir(s.cfg.CacheDir)
	if e != nil || len(files) != 2 {
		t.Fatal("real image destination absent", len(files), e)
	}
	s.executeImage(work{ctx: context.Background(), imageDoc: id, imageAsset: asset, task: Task{Mode: "alice", Width: 32, Height: 32}, result: result})
	if out := <-result; code(out.err) != "busy" || dials.Load() != 1 {
		t.Fatal("uncertain capacity admitted another image fetch", out.err, dials.Load())
	}

	s.mu.Lock()
	s.cacheSyncDir = cacheDirSync
	s.expire()
	after = s.disk
	s.mu.Unlock()
	if after != before {
		t.Fatal("image cleanup capacity leak", after, before)
	}
}
