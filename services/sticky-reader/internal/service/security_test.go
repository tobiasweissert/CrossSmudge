package service

import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"hash/crc32"
	"image"
	"image/jpeg"
	"image/png"
	"io"
	"math/big"
	"net"
	"net/http"
	"strings"
	"testing"
	"time"
)

func TestMappedDNSAndPeerMismatch(t *testing.T) {
	f, dns, dials := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { t.Error("should never connect") }))
	dns.answers = []string{"::ffff:93.184.216.34"}
	if _, e := f.Fetch(context.Background(), "https://example.com/", "html"); code(e) != "address_denied" || dials.Load() != 0 {
		t.Fatal("mapped DNS bypass", e)
	}
	dns.answers = []string{"93.184.216.34"}
	dial := f.Dial
	f.Dial = func(c context.Context, n, a string) (net.Conn, error) {
		conn, e := dial(c, n, a)
		if e != nil {
			return nil, e
		}
		return fakePeer{conn, "10.0.0.1"}, nil
	}
	if _, e := f.Fetch(context.Background(), "https://example.com/", "html"); e == nil {
		t.Fatal("wrong peer")
	}
}
func TestRedirectBudgetAndHeaders(t *testing.T) {
	f, _, dials := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.Header().Set("Location", "/again"); w.WriteHeader(302) }))
	if _, e := f.Fetch(context.Background(), "https://example.com/again", "html"); e == nil || dials.Load() != 5 {
		t.Fatal("redirect cap", e, dials.Load())
	}
	f, _, _ = fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		w.Header().Set("Content-Length", "1048577")
		w.WriteHeader(200)
	}))
	if _, e := f.Fetch(context.Background(), "https://example.com/", "html"); code(e) != "too_large" {
		t.Fatal(e)
	}
	f, _, _ = fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		w.Write(bytes.Repeat([]byte("x"), 1048577))
	}))
	if _, e := f.Fetch(context.Background(), "https://example.com/", "html"); code(e) != "too_large" {
		t.Fatal(e)
	}
}
func TestStaticVisibilityFormsAndFragments(t *testing.T) {
	d, e := convertHTML([]byte(`<title>Original</title><style>.secret{visibility:hidden}</style><p>Prefix <span id="visible">target</span><a href="/next">Next</a></p><p class="secret" id="hidden">Secret</p><form method="POST" action="/login"><input type="password" name="p"></form><form action="/search"><input name="q" type="search"><input name="limit" type="hidden" value="5"></form><p>Tail retained.</p>`), "https://example.com/", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	b, _ := json.Marshal(d)
	if bytes.Contains(b, []byte("Secret")) || bytes.Contains(b, []byte("login")) {
		t.Fatal("active/hidden retained")
	}
	forms := 0
	for _, c := range d.Chunks {
		for _, block := range c.Blocks {
			if block.Form != nil {
				forms++
			}
		}
		for _, a := range c.Anchors {
			if a.ID == "hidden" {
				t.Fatal("hidden anchor")
			}
		}
	}
	if forms != 1 {
		t.Fatal("GET form lost")
	}
}
func TestImageLimitsAndWhiteAlpha(t *testing.T) {
	im := image.NewNRGBA(image.Rect(0, 0, 32, 32))
	var raw bytes.Buffer
	png.Encode(&raw, im)
	out, e := convertImage(raw.Bytes(), 800, 800)
	if e != nil {
		t.Fatal(e)
	}
	decoded, e := png.Decode(bytes.NewReader(out))
	if e != nil {
		t.Fatal(e)
	}
	if decoded.Bounds().Dx() != 32 {
		t.Fatal("upscale")
	}
	for y := 0; y < 32; y++ {
		for x := 0; x < 32; x++ {
			r, _, _, _ := decoded.At(x, y).RGBA()
			if r != 65535 {
				t.Fatal("alpha not white")
			}
		}
	}
}
func TestImageBombCorruptionAndJPEG(t *testing.T) {
	im := image.NewGray(image.Rect(0, 0, 64, 32))
	for i := range im.Pix {
		im.Pix[i] = 128
	}
	var b bytes.Buffer
	jpeg.Encode(&b, im, nil)
	out, e := convertImage(b.Bytes(), 32, 32)
	if e != nil || out[24] != 1 {
		t.Fatal("JPEG", e)
	}
	b.Reset()
	png.Encode(&b, im)
	bomb := append([]byte(nil), b.Bytes()...)
	binary.BigEndian.PutUint32(bomb[16:20], 4096)
	binary.BigEndian.PutUint32(bomb[20:24], 4096)
	binary.BigEndian.PutUint32(bomb[29:33], crc32.ChecksumIEEE(bomb[12:29]))
	if _, e := convertImage(bomb, 100, 100); code(e) != "too_large" {
		t.Fatal("pixel bomb", e)
	}
	for _, raw := range [][]byte{[]byte("GIF89a"), b.Bytes()[:30], []byte("<svg/>"), bytes.Repeat([]byte("x"), 4<<20+1)} {
		if _, e := convertImage(raw, 10, 10); e == nil {
			t.Fatal("bad image")
		}
	}
}
func TestRequestDepthAndControls(t *testing.T) {
	for _, s := range []string{`{"x":` + strings.Repeat("[", 13) + `1` + strings.Repeat("]", 13) + `}`, `{"x":"\u0001"}`, `{"x":"\udc00"}`, `{"x":"\ud800x"}`, `{"x":1000000}`} {
		var v any
		if strictJSON([]byte(s), &v) == nil {
			t.Fatalf("accepted %s", s)
		}
	}
	var v any
	if e := strictJSON([]byte(`{"x":"\ud83d\ude00\t\n"}`), &v); e != nil {
		t.Fatal("valid unicode", e)
	}
}
func TestDeadlineAndNoProxy(t *testing.T) {
	t.Setenv("HTTPS_PROXY", "http://127.0.0.1:1")
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		io.WriteString(w, "<p>original</p>")
	}))
	if _, e := f.Fetch(context.Background(), "https://example.com/", "html"); e != nil {
		t.Fatal("env proxy used", e)
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Nanosecond)
	defer cancel()
	if _, e := f.Fetch(ctx, "https://example.com/", "html"); e == nil {
		t.Fatal("deadline ignored")
	}
}

func TestDuplicateTextAnchorsAndFragmentHref(t *testing.T) {
	d, e := convertHTML([]byte(`<p id="one">Identical text.</p><p id="two">Identical text.</p><p><a href="#two">Jump</a></p>`), "https://example.com/story", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	got := map[string]int{}
	href := ""
	for _, c := range d.Chunks {
		for _, a := range c.Anchors {
			got[a.ID] = a.Block
		}
		for _, b := range c.Blocks {
			for _, r := range b.Runs {
				if r.Href != "" {
					href = r.Href
				}
			}
		}
	}
	if got["one"] == got["two"] || got["two"] != 2 {
		t.Fatal("duplicate text target fabricated", got)
	}
	if href != "https://example.com/story#two" {
		t.Fatal("fragment link lost", href)
	}
}

func TestFailedFetchChargesQuota(t *testing.T) {
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		w.WriteHeader(403)
		w.Write(bytes.Repeat([]byte("x"), 1000))
	}))
	s := newTestServer(t, f)
	w := request(t, s, "/v1/open", `{"v":1,"rid":"`+strings.Repeat("9", 32)+`","url":"https://example.com/","mode":"reader","images":"on-demand"}`, "a")
	if w.Code != 202 {
		t.Fatal(w.Code)
	}
	s.mu.Lock()
	j := s.rid["alice:"+strings.Repeat("9", 32)]
	s.mu.Unlock()
	select {
	case <-j.Done:
	case <-time.After(time.Second):
		t.Fatal("job stuck")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.rates["alice"].Bytes != 1000 {
		t.Fatal("failed fetch bypassed quota", s.rates["alice"].Bytes)
	}
}

// Original 16x16 uniform mid-gray, encoded losslessly with ffmpeg/libwebp; no external media.
func TestActualWebPDecode(t *testing.T) {
	raw, e := base64.StdEncoding.DecodeString("UklGRh4AAABXRUJQVlA4TBEAAAAvD8ADAAdQwAIWsP+BiOh/AAA=")
	if e != nil {
		t.Fatal(e)
	}
	out, e := convertImage(raw, 16, 16)
	if e != nil {
		t.Fatal("WebP", e)
	}
	im, e := png.Decode(bytes.NewReader(out))
	if e != nil {
		t.Fatal(e)
	}
	black := 0
	for y := 0; y < 16; y++ {
		for x := 0; x < 16; x++ {
			r, _, _, _ := im.At(x, y).RGBA()
			if r == 0 {
				black++
			}
		}
	}
	if black < 115 || black > 140 {
		t.Fatal("midtone density", black)
	}
}
func TestFailedGzipCountsDecodedBudget(t *testing.T) {
	var b bytes.Buffer
	g := gzip.NewWriter(&b)
	g.Write(bytes.Repeat([]byte("x"), 1000))
	g.Close()
	raw := append([]byte(nil), b.Bytes()...)
	raw[len(raw)-5] ^= 1
	f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		w.Header().Set("Content-Encoding", "gzip")
		w.Write(raw)
	}))
	out, e := f.Fetch(context.Background(), "https://example.com/", "html")
	if e == nil || out.Decoded != 1000 {
		t.Fatal("failed decode uncharged", out.Decoded, e)
	}
}

func TestTLSWrongHostAndExpired(t *testing.T) {
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html")
		io.WriteString(w, "Original")
	})
	f, _, _ := fixtureFetcher(t, handler)
	if _, e := f.Fetch(context.Background(), "https://wrong.example.org/", "html"); code(e) != "tls_failed" {
		t.Fatal("wrong host accepted", e)
	}
	key, e := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if e != nil {
		t.Fatal(e)
	}
	template := &x509.Certificate{SerialNumber: big.NewInt(1), DNSNames: []string{"example.com"}, NotBefore: time.Now().Add(-48 * time.Hour), NotAfter: time.Now().Add(-24 * time.Hour), KeyUsage: x509.KeyUsageDigitalSignature, ExtKeyUsage: []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth}}
	der, e := x509.CreateCertificate(rand.Reader, template, template, &key.PublicKey, key)
	if e != nil {
		t.Fatal(e)
	}
	f, _, _ = fixtureFetcher(t, handler, &tls.Config{Certificates: []tls.Certificate{{Certificate: [][]byte{der}, PrivateKey: key}}})
	if _, e := f.Fetch(context.Background(), "https://example.com/", "html"); code(e) != "tls_failed" {
		t.Fatal("expired accepted", e)
	}
}

func TestVisibleIndexLinks(t *testing.T) {
	for _, mode := range []string{"reader", "simplified"} {
		d, e := convertHTML([]byte(`<title>Original story</title><nav><a href="/index">Original index</a></nav><article><h1>Original story</h1><p>`+strings.Repeat("Original article text retained. ", 40)+`</p></article>`), "https://example.com/", mode)
		if e != nil {
			t.Fatal(e)
		}
		found := false
		for _, c := range d.Chunks {
			for _, b := range c.Blocks {
				for _, r := range b.Runs {
					if r.Href == "https://example.com/index" {
						found = true
					}
				}
			}
		}
		if !found {
			t.Fatal(mode, "visible index link lost")
		}
	}
}

func TestReaderPreservesShortIndexCards(t *testing.T) {
	var source strings.Builder
	source.WriteString(`<title>Original index</title><main>`)
	for i := 0; i < 36; i++ {
		fmt.Fprintf(&source, `<article><a href="/story%d"><h2>ORIGINAL_CARD_%02d</h2><span>Original bounded index summary with enough visible words for extraction.</span></a></article>`, i, i)
	}
	source.WriteString(`</main>`)
	d, e := convertHTML([]byte(source.String()), "https://example.com/", "reader")
	if e != nil {
		t.Fatal(e)
	}
	var text strings.Builder
	for _, c := range d.Chunks {
		for _, b := range c.Blocks {
			for _, r := range b.Runs {
				text.WriteString(r.Text)
			}
		}
	}
	for i := 0; i < 36; i++ {
		if !strings.Contains(text.String(), fmt.Sprintf("ORIGINAL_CARD_%02d", i)) {
			t.Fatal("index card lost", i)
		}
	}
}

func TestDocumentAggregateTextBudget(t *testing.T) {
	source := `<title>` + strings.Repeat("T", 200) + `</title><article>`
	for i := 0; i < 80; i++ {
		source += `<p>` + strings.Repeat("Original long café content. ", 140) + `<a href="/link">Link</a></p>`
	}
	source += `</article>`
	d, e := convertHTML([]byte(source), "https://example.com/", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	total := 0
	for _, c := range d.Chunks {
		total += chunkText(c)
	}
	if total > 256<<10 || !d.Chunks[0].Truncated {
		t.Fatal("document decoded text budget", total)
	}
}
func TestKnownNodeAddressDenied(t *testing.T) {
	if publicIP("46.225.216.130") {
		t.Fatal("known hosting node may not be fetched through alias DNS")
	}
}

func TestFindAcrossStyledRuns(t *testing.T) {
	d, e := convertHTML([]byte(`<p>Hello <strong>WORLD café</strong> tail. Hello <em>world</em> again.</p>`), "https://example.com/", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	first := find(d, "hello world", nil)
	if !first.Found || first.Run != 1 || first.Byte != 1 {
		t.Fatal("styled literal search", first)
	}
	next := find(d, "hello world", &Position{*first.Chunk, first.Block, first.Run, first.Byte})
	if !next.Found || next.Block != first.Block || next.Run <= first.Run {
		t.Fatal("exclusive next match", next)
	}
	if !find(d, "CAFé tail", nil).Found || find(d, "CAFÉ tail", nil).Found {
		t.Fatal("ASCII-only case folding")
	}
}
