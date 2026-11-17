package service

import (
	"bytes"
	"encoding/json"
	"fmt"
	"image"
	"image/png"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

func TestPublicFlowAndLegacyHeaders(t *testing.T) {
	var pngBytes bytes.Buffer
	png.Encode(&pngBytes, image.NewGray(image.Rect(0, 0, 16, 16)))
	var legacyFetched atomic.Bool
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/legacy" {
			legacyFetched.Store(true)
		}
		if r.Header.Get("Authorization") != "" || r.Header.Get("X-Device-ID") != "" {
			t.Error("public identity forwarded upstream")
		}
		if r.URL.Path == "/photo.png" {
			w.Header().Set("Content-Type", "image/png")
			w.Write(pngBytes.Bytes())
			return
		}
		w.Header().Set("Content-Type", "text/html")
		fmt.Fprint(w, `<html><title>Public original</title><p id="top">`+strings.Repeat("Original public passage. ", 500)+`</p><p id="end">PUBLIC_TAIL</p><img src="/photo.png" alt="original" width="16" height="16"></html>`)
	}))
	s, e := New(Config{CacheDir: t.TempDir(), Fetcher: f, WorkerArgs: []string{"-test.run=^TestWorkerProcess$"}, WorkerEnv: []string{"SRP_TEST_WORKER=1"}})
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	rid := strings.Repeat("3", 32)
	open := `{"v":1,"rid":"` + rid + `","url":"https://example.com/story","mode":"simplified","images":"on-demand"}`
	w := request(t, s, "/v1/open", open, "")
	if w.Code != 202 {
		t.Fatal(w.Code, w.Body)
	}
	var pending struct{ Job string }
	json.Unmarshal(w.Body.Bytes(), &pending)
	var c Chunk
	deadline := time.Now().Add(5 * time.Second)
	for {
		w = request(t, s, "/v1/poll", fmt.Sprintf(`{"v":1,"job":%q}`, pending.Job), "")
		if w.Code == 200 {
			json.Unmarshal(w.Body.Bytes(), &c)
			break
		}
		if w.Code != 202 || time.Now().After(deadline) {
			t.Fatal(w.Code, w.Body)
		}
		time.Sleep(500 * time.Millisecond)
	}
	asset := ""
	for n := 0; n < c.Chunks; n++ {
		w = request(t, s, "/v1/chunk", fmt.Sprintf(`{"v":1,"doc":%q,"chunk":%d}`, c.Doc, n), "")
		if w.Code != 200 {
			t.Fatal(w.Code, w.Body)
		}
		var ch Chunk
		json.Unmarshal(w.Body.Bytes(), &ch)
		for _, b := range ch.Blocks {
			if b.Image != nil {
				asset = b.Image.Asset
			}
		}
	}
	if asset == "" {
		t.Fatal("image lost")
	}
	for _, v := range [][2]string{{"locate", fmt.Sprintf(`{"v":1,"doc":%q,"fragment":"end"}`, c.Doc)}, {"find", fmt.Sprintf(`{"v":1,"doc":%q,"text":"public_tail","after":null}`, c.Doc)}} {
		w = request(t, s, "/v1/"+v[0], v[1], "")
		if w.Code != 200 || !strings.Contains(w.Body.String(), `"found":true`) {
			t.Fatal(w.Code, w.Body)
		}
	}
	w = request(t, s, "/v1/image", fmt.Sprintf(`{"v":1,"doc":%q,"asset":%q,"width":801,"height":16}`, c.Doc, asset), "")
	if w.Code != 400 {
		t.Fatal("public image bounds", w.Code, w.Body)
	}
	w = request(t, s, "/v1/image", fmt.Sprintf(`{"v":1,"doc":%q,"asset":%q,"width":16,"height":16}`, c.Doc, asset), "")
	if w.Code != 200 || w.Header().Get("Content-Type") != "image/png" {
		t.Fatal(w.Code, w.Body)
	}
	// A forged legacy identity cannot isolate content or acquire another budget.
	w = request(t, s, "/v1/open", open, "b")
	if w.Code != 200 {
		t.Fatal("legacy auth changed principal", w.Code, w.Body)
	}
	// A fresh legacy-auth open really fetches (not merely an idempotent hit),
	// yet its Authorization never reaches the TLS publisher fixture.
	legacy := strings.Replace(strings.Replace(open, rid, strings.Repeat("5", 32), 1), "/story", "/legacy", 1)
	w = request(t, s, "/v1/open", legacy, "b")
	if w.Code != 202 {
		t.Fatal(w.Code, w.Body)
	}
	json.Unmarshal(w.Body.Bytes(), &pending)
	deadline = time.Now().Add(5 * time.Second)
	for {
		w = request(t, s, "/v1/poll", fmt.Sprintf(`{"v":1,"job":%q}`, pending.Job), "")
		if w.Code == 200 {
			break
		}
		if w.Code != 202 || time.Now().After(deadline) {
			t.Fatal(w.Code, w.Body)
		}
		time.Sleep(500 * time.Millisecond)
	}
	if !legacyFetched.Load() {
		t.Fatal("legacy open did not fetch")
	}
	other := strings.Repeat("4", 32)
	w = request(t, s, "/v1/cancel", fmt.Sprintf(`{"v":1,"rid":%q}`, other), "")
	if w.Code != 200 {
		t.Fatal(w.Code)
	}
	w = request(t, s, "/v1/open", strings.Replace(open, rid, other, 1), "a")
	if w.Code != 410 {
		t.Fatal("legacy identity escaped cancel tombstone", w.Code)
	}
	if len(s.rates) != 1 {
		t.Fatal("public principals grew")
	}
}

func TestPublicHeadersCannotMintBudget(t *testing.T) {
	s, e := New(Config{CacheDir: t.TempDir()})
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	// Repeated same RID is idempotent but consumes the fixed request budget.
	body := `{"v":1,"rid":"00000000000000000000000000000000"}`
	for i := 0; i < 65; i++ {
		r := httptest.NewRequest("POST", "/v1/cancel", strings.NewReader(body))
		r.Header.Set("Content-Type", "application/json")
		r.Header.Set("Authorization", fmt.Sprintf("Bearer %064x", i))
		r.Header.Set("X-Device-ID", fmt.Sprint(i))
		r.Header.Set("X-Forwarded-For", fmt.Sprintf("93.184.216.%d", i))
		w := httptest.NewRecorder()
		s.ServeHTTP(w, r)
		want := 200
		if i >= 60 {
			want = 429
		}
		if w.Code != want {
			t.Fatalf("request %d: got %d want %d", i, w.Code, want)
		}
	}
	if len(s.rates) != 1 {
		t.Fatal("header-controlled principal map")
	}
}
