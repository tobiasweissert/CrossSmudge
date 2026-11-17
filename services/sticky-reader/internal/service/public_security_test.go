package service

import (
	"net/http/httptest"
	"strings"
	"testing"
)

func TestPublicAdmissionDenials(t *testing.T) {
	s, e := New(Config{CacheDir: t.TempDir()})
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	for _, c := range []struct {
		name, path, body, header, value string
		want                            int
	}{
		{"Origin", "cancel", `{"v":1,"rid":"00000000000000000000000000000000"}`, "Origin", "https://evil.example", 403},
		{"body", "cancel", strings.Repeat("x", 2049), "", "", 413},
		{"headers", "cancel", "{}", "X-Device-ID", strings.Repeat("x", 8193), 413},
		{"encoding", "cancel", "{}", "Content-Encoding", "gzip", 400},
		{"JSON", "cancel", `{"v":1,"v":1}`, "", "", 400},
		{"SSRF", "open", `{"v":1,"rid":"00000000000000000000000000000001","url":"https://127.0.0.1/","mode":"reader","images":"on-demand"}`, "", "", 403},
		{"unknown image document", "image", `{"v":1,"doc":"00000000000000000000000000000001","asset":"00000000000000000000000000000002","width":801,"height":1}`, "", "", 404},
	} {
		t.Run(c.name, func(t *testing.T) {
			r := httptest.NewRequest("POST", "/v1/"+c.path, strings.NewReader(c.body))
			r.Header.Set("Content-Type", "application/json")
			if c.header != "" {
				r.Header.Set(c.header, c.value)
			}
			w := httptest.NewRecorder()
			s.ServeHTTP(w, r)
			if w.Code != c.want {
				t.Fatalf("got %d want %d: %s", w.Code, c.want, w.Body)
			}
		})
	}
}
