package service

import (
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

var cacheName = regexp.MustCompile(`^[0-9a-f]{64}$`)

// ResetCache is for the dedicated ephemeral app directory only. A restart invalidates all IDs.
// Refuse unknown entries and links instead of deleting arbitrary operator files.
func ResetCache(dir string) error {
	if e := os.MkdirAll(dir, 0700); e != nil {
		return e
	}
	st, e := os.Lstat(dir)
	if e != nil || !st.IsDir() || st.Mode()&os.ModeSymlink != 0 {
		return fail("busy")
	}
	f, e := os.Open(dir)
	if e != nil {
		return e
	}
	files, e := f.ReadDir(4097)
	f.Close()
	if e != nil && e != io.EOF || len(files) > 4096 {
		return fail("busy")
	}
	for _, ent := range files {
		if !ent.Type().IsRegular() || !cacheName.MatchString(ent.Name()) && !strings.HasPrefix(ent.Name(), ".part-") {
			return fail("busy")
		}
	}
	for _, ent := range files {
		if e := os.Remove(filepath.Join(dir, ent.Name())); e != nil {
			return e
		}
	}
	return cacheDirSync(dir)
}
