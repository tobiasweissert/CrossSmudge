package service

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"image"
	"image/color"
	"image/png"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
	"time"
)

func TestStrictJSON(t *testing.T) {
	for _, s := range []string{`{"v":1,"v":1}`, `{"v":1} false`, `{"x":"\ud800"}`, `{"x":"\u0000"}`, `{"x":1.2}`, "{\"x\":\"\xff\"}"} {
		var v map[string]json.RawMessage
		if strictJSON([]byte(s), &v) == nil {
			t.Errorf("accepted %q", s)
		}
	}
}
func TestURLPolicy(t *testing.T) {
	for _, s := range []string{"http://example.org/", "https://127.0.0.1/", "https://2130706433/", "https://0x7f000001/", "https://example.org:443/", "https://example.org./", "https://u:p@example.org/", "https://nytimes.com/", "https://x.nytimg.com/", "https://sticky-browser.tobiasweissert.ch/", "https://localhost/", "https://example.org/%00"} {
		if _, e := safeURL(s); e == nil {
			t.Errorf("accepted %s", s)
		}
	}
	for _, s := range []string{"127.0.0.1", "10.0.0.1", "100.64.0.1", "169.254.169.254", "192.0.2.1", "198.18.0.1", "224.0.0.1", "::ffff:8.8.8.8", "::1", "2001:db8::1", "fc00::1"} {
		if publicIP(s) {
			t.Errorf("public %s", s)
		}
	}
	if !publicIP("93.184.216.34") {
		t.Fatal("public rejected")
	}
}
func TestSemanticAndImages(t *testing.T) {
	input := `<html lang="en"><title>Synthetic navigation</title><body><nav><a href="/index">Index</a></nav><article><h1 id="top">Original</h1><p id="begin">` + strings.Repeat("Beginning middle UTF8 café. ", 1000) + `</p><p id="end">FINAL_SENTINEL <a href="/next">Next</a></p><p hidden id="gone">HIDDEN_SENTINEL</p><img src="/photo.png" width="200" height="100" alt="Original gradient"><form action="/search"><input name="q" type="search"></form></article><script>` + strings.Repeat("x", 20000) + `</script></body></html>`
	for _, mode := range []string{"simplified", "reader"} {
		d, e := convertHTML([]byte(input), "https://example.org/story", mode)
		if e != nil {
			t.Fatal(e)
		}
		raw, _ := json.Marshal(d)
		if !bytes.Contains(raw, []byte("FINAL_SENTINEL")) || bytes.Contains(raw, []byte("HIDDEN_SENTINEL")) {
			t.Fatalf("%s lost tail or hidden leak", mode)
		}
		if len(d.Chunks) < 3 {
			t.Fatalf("no real chunking: %d", len(d.Chunks))
		}
		for _, c := range d.Chunks {
			b, _ := json.Marshal(c)
			if len(b) > 49152 || chunkText(c) > 8192 {
				t.Fatal("chunk overflow")
			}
		}
	}
	im := image.NewNRGBA(image.Rect(0, 0, 256, 100))
	for y := 0; y < 100; y++ {
		for x := 0; x < 256; x++ {
			im.SetNRGBA(x, y, color.NRGBA{uint8(x), uint8(x), uint8(x), 255})
		}
	}
	var b bytes.Buffer
	png.Encode(&b, im)
	out, e := convertImage(b.Bytes(), 128, 100)
	if e != nil {
		t.Fatal(e)
	}
	if out[24] != 1 || out[25] != 3 {
		t.Fatalf("not 1 bit palette %v", out[24:26])
	}
	decoded, e := png.Decode(bytes.NewReader(out))
	if e != nil {
		t.Fatal(e)
	}
	if decoded.Bounds().Dx() != 128 || decoded.Bounds().Dy() != 50 {
		t.Fatal("aspect ratio")
	}
	black := 0
	for y := 0; y < 50; y++ {
		for x := 0; x < 128; x++ {
			r, _, _, _ := decoded.At(x, y).RGBA()
			if r == 0 {
				black++
			}
		}
	}
	if black < 2800 || black > 3600 {
		t.Fatalf("density %d", black)
	}
	out2, _ := convertImage(b.Bytes(), 128, 100)
	if !bytes.Equal(out, out2) {
		t.Fatal("not deterministic")
	}
}
func TestWorkerProcess(t *testing.T) {
	if os.Getenv("SRP_TEST_WORKER") == "1" {
		os.Exit(WorkerMain())
	}
}
func newTestServer(t *testing.T, f *Fetcher) *Server {
	t.Helper()
	a := sha256.Sum256([]byte(strings.Repeat("a", 64)))
	b := sha256.Sum256([]byte(strings.Repeat("b", 64)))
	s, e := New(Config{CacheDir: t.TempDir(), Verifiers: map[string][32]byte{"alice": a, "bob": b}, Fetcher: f, WorkerArgs: []string{"-test.run=^TestWorkerProcess$"}, WorkerEnv: []string{"SRP_TEST_WORKER=1"}})
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(s.Close)
	return s
}
func request(t *testing.T, s http.Handler, path, body, token string) *httptest.ResponseRecorder {
	t.Helper()
	r := httptest.NewRequest("POST", path, strings.NewReader(body))
	r.Header.Set("Content-Type", "application/json")
	if token != "" {
		r.Header.Set("Authorization", "Bearer "+strings.Repeat(token, 64))
	}
	w := httptest.NewRecorder()
	s.ServeHTTP(w, r)
	return w
}
func TestHTTPAuthStrictCancel(t *testing.T) {
	s := newTestServer(t, nil)
	if w := request(t, s, "/v1/open", `bad`, ""); w.Code != 401 {
		t.Fatal(w.Code)
	}
	rid := hex.EncodeToString(make([]byte, 16))
	if w := request(t, s, "/v1/cancel", `{"v":1,"rid":"`+rid+`"}`, "a"); w.Code != 200 {
		t.Fatal(w.Code, w.Body.String())
	}
	body := `{"v":1,"rid":"` + rid + `","url":"https://example.org/","mode":"reader","images":"on-demand"}`
	if w := request(t, s, "/v1/open", body, "a"); w.Code != 410 {
		t.Fatal(w.Code, w.Body.String())
	}
	for _, body := range []string{`{"v":1,"rid":"` + rid + `","extra":1}`, `{"v":1,"v":1,"rid":"` + rid + `"}`, `{"v":1,"rid":12}`} {
		if w := request(t, s, "/v1/cancel", body, "a"); w.Code != 400 {
			t.Fatal(w.Code)
		}
	}
	if w := request(t, s, "/v1/chunk", `{"v":1,"doc":"`+rid+`","chunk":0}`, "b"); w.Code != 404 {
		t.Fatal(w.Code)
	}
}
func TestOwnedChildCancellation(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Millisecond)
	defer cancel()
	_, e := runChild(ctx, Task{Kind: "html", Data: []byte(strings.Repeat("<div>", 100000)), URL: "https://example.org/", Mode: "reader"}, nil, nil)
	if e == nil {
		t.Fatal("expected cancellation or bounded rejection")
	}
}
