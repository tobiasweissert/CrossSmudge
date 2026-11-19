package service

import (
	"fmt"
	"net/http/httptest"
	"strings"
	"testing"
)

// Nil verifier configuration is the production public mode. Explicit verifier
// maps remain a library-only private-mode regression seam.
func TestPublicAnonymousAdmission(t *testing.T) {
	s, e := New(Config{CacheDir: t.TempDir()})
	if e != nil {
		t.Fatalf("public startup without verifier: %v", e)
	}
	defer s.Close()
	for i, auth := range []string{"", "Bearer " + strings.Repeat("a", 64), "invented"} {
		r := httptest.NewRequest("POST", "/v1/cancel", strings.NewReader(fmt.Sprintf(`{"v":1,"rid":"%032x"}`, i)))
		r.Header.Set("Content-Type", "application/json")
		r.Header.Set("Origin", "https://sticky.tobiasweissert.ch")
		r.Header.Set("Authorization", auth)
		r.Header.Set("X-Forwarded-For", fmt.Sprint(i))
		r.Header.Set("X-Device-ID", fmt.Sprint(i))
		w := httptest.NewRecorder()
		s.ServeHTTP(w, r)
		if w.Code != 200 {
			t.Fatalf("public cancel %d: %d %s", i, w.Code, w.Body)
		}
	}
	if len(s.rates) != 1 {
		t.Fatalf("unbounded/header-selected principals: %d", len(s.rates))
	}
	r := httptest.NewRequest("POST", "/v1/cancel", strings.NewReader(`{"v":1,"rid":"00000000000000000000000000000001"}`))
	r.Header.Set("Origin", "https://evil.example")
	r.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	s.ServeHTTP(w, r)
	if w.Code != 403 {
		t.Fatalf("Origin bypass: %d", w.Code)
	}
}
