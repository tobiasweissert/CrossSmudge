// qualify runs production HTTP/fetch/extraction/codec paths with explicit fake DNS and connection boundaries.
// It never connects to the internet or supplies production credentials.
package main

import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/sha256"
	"crypto/x509"
	"encoding/json"
	"fmt"
	"image"
	"image/png"
	"io"
	"log"
	"math/rand"
	"net"
	"net/http"
	"net/http/httptest"
	"net/netip"
	"os"
	"runtime"
	"runtime/debug"
	"strings"
	"sync"
	"time"

	"sticky-reader/internal/service"
)

type listener struct {
	c    chan net.Conn
	done chan struct{}
	once sync.Once
}

func (l *listener) Accept() (net.Conn, error) {
	select {
	case c := <-l.c:
		return c, nil
	case <-l.done:
		return nil, net.ErrClosed
	}
}
func (l *listener) Close() error   { l.once.Do(func() { close(l.done) }); return nil }
func (l *listener) Addr() net.Addr { return &net.TCPAddr{IP: net.IPv4(127, 0, 0, 1), Port: 1} }
func (l *listener) dial(ctx context.Context, n, a string) (net.Conn, error) {
	x, y := net.Pipe()
	select {
	case l.c <- x:
		return y, nil
	case <-ctx.Done():
		x.Close()
		y.Close()
		return nil, ctx.Err()
	case <-l.done:
		x.Close()
		y.Close()
		return nil, net.ErrClosed
	}
}
func fixture(h http.Handler, tlsMode bool) *httptest.Server {
	l := &listener{make(chan net.Conn), make(chan struct{}), sync.Once{}}
	s := &httptest.Server{Listener: l, Config: &http.Server{Handler: h, ErrorLog: log.New(io.Discard, "", 0)}}
	if tlsMode {
		s.StartTLS()
	} else {
		s.Start()
	}
	s.Client().Transport.(*http.Transport).DialContext = l.dial
	return s
}

type dns struct{}

func (dns) LookupNetIP(context.Context, string, string) ([]netip.Addr, error) {
	return []netip.Addr{netip.MustParseAddr("93.184.216.34")}, nil
}

type peer struct{ net.Conn }

func (p peer) RemoteAddr() net.Addr { return &net.TCPAddr{IP: net.ParseIP("93.184.216.34"), Port: 443} }
func request(client *http.Client, origin, path string, v any) (int, []byte, error) {
	b, _ := json.Marshal(v)
	r, _ := http.NewRequest("POST", origin+path, bytes.NewReader(b))
	r.Header.Set("Content-Type", "application/json")
	r.Header.Set("Authorization", "Bearer "+strings.Repeat("a", 64))
	resp, e := client.Do(r)
	if e != nil {
		return 0, nil, e
	}
	defer resp.Body.Close()
	data, e := io.ReadAll(resp.Body)
	return resp.StatusCode, data, e
}
func check(err error) {
	if err != nil {
		panic("qualification failed: " + err.Error())
	}
}
func main() {
	if len(os.Args) == 2 && os.Args[1] == "--worker" {
		os.Exit(service.WorkerMain())
	}
	runtime.GOMAXPROCS(1)
	debug.SetMemoryLimit(48 << 20)
	if e := run(); e != nil {
		fmt.Fprintln(os.Stderr, "qualification failed:", e)
		os.Exit(1)
	}
}
func run() error {
	var html strings.Builder
	html.WriteString(`<html lang="en"><title>Original resource fixture</title><link rel="stylesheet" href="/style.css"><body><nav><a href="/index">Index</a></nav><article><h1 id="begin">Original beginning</h1>`)
	for i := 0; i < 64; i++ {
		fmt.Fprintf(&html, `<p id="p%d">Paragraph %d `, i, i)
		html.WriteString(strings.Repeat("Original synthetic café passage with retained words. ", 55))
		html.WriteString("</p>")
	}
	html.WriteString(`<p id="end">FINAL_ORIGINAL_SENTINEL</p><img src="/image.png" width="2000" height="2000" alt="Original synthetic ramp"><form action="/search"><input name="q" type="search"></form></article><p class="hidden">HIDDEN_SENTINEL</p><script type="application/json">`)
	rng := rand.New(rand.NewSource(42))
	for i := 0; i < 400000; i++ {
		html.WriteByte(byte('a' + rng.Intn(26)))
	}
	html.WriteString(strings.Repeat("inertJSONdata", 220000))
	html.WriteString("</script></body></html>")
	var compressed bytes.Buffer
	gz := gzip.NewWriter(&compressed)
	gz.Write([]byte(html.String()))
	gz.Close()
	originalHTML := html.Len()
	im := image.NewNRGBA64(image.Rect(0, 0, 2000, 2000))
	for y := 0; y < 2000; y++ {
		for x := 0; x < 2000; x++ {
			v := uint16(x * 65535 / 1999)
			i := y*im.Stride + x*8
			for channel := 0; channel < 3; channel++ {
				im.Pix[i+channel*2] = byte(v >> 8)
				im.Pix[i+channel*2+1] = byte(v)
			}
			im.Pix[i+6] = 255
			im.Pix[i+7] = 255
		}
	}
	var imageData bytes.Buffer
	png.Encode(&imageData, im)
	im = nil
	runtime.GC()
	capture := []byte(nil)
	if path := os.Getenv("SRP_CAPTURE"); path != "" {
		var e error
		capture, e = os.ReadFile(path)
		if e != nil {
			return e
		}
	}
	source := fixture(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Authorization") != "" || r.Header.Get("Cookie") != "" {
			panic("credentials forwarded")
		}
		switch r.URL.Path {
		case "/image.png":
			w.Header().Set("Content-Type", "image/png")
			w.Write(imageData.Bytes())
		case "/style.css":
			w.Header().Set("Content-Type", "text/css")
			io.WriteString(w, ".hidden{display:none}")
		case "/slow":
			<-r.Context().Done()
		case "/capture":
			w.Header().Set("Content-Type", "text/html")
			w.Header().Set("Content-Encoding", "gzip")
			g := gzip.NewWriter(w)
			g.Write(capture)
			g.Close()
		default:
			w.Header().Set("Content-Type", "text/html")
			w.Header().Set("Content-Encoding", "gzip")
			w.Write(compressed.Bytes())
		}
	}), true)
	defer source.Close()
	roots := x509.NewCertPool()
	roots.AddCert(source.Certificate())
	fetcher := &service.Fetcher{Resolver: dns{}, Roots: roots, Dial: func(ctx context.Context, n, a string) (net.Conn, error) {
		if a != "93.184.216.34:443" {
			return nil, fmt.Errorf("numeric pin missing")
		}
		c, e := source.Client().Transport.(*http.Transport).DialContext(ctx, n, a)
		if e != nil {
			return nil, e
		}
		return peer{c}, nil
	}}
	dir, e := os.MkdirTemp(os.Getenv("TMPDIR"), "srp-qualify-")
	if e != nil {
		return e
	}
	defer os.RemoveAll(dir)
	hash := sha256.Sum256([]byte(strings.Repeat("a", 64)))
	server, e := service.New(service.Config{CacheDir: dir, Verifiers: map[string][32]byte{"synthetic-only": hash}, Fetcher: fetcher})
	if e != nil {
		return e
	}
	defer server.Close()
	api := fixture(server, false)
	defer api.Close()
	client := api.Client()
	summaries := []map[string]any{}
	paths := []string{"/original"}
	if len(capture) > 0 {
		paths = append(paths, "/capture")
	}
	for index, path := range paths {
		start := time.Now()
		rid := fmt.Sprintf("%032x", index+1)
		status, b, e := request(client, api.URL, "/v1/open", map[string]any{"v": 1, "rid": rid, "url": "https://example.com" + path, "mode": "reader", "images": "on-demand"})
		if e != nil || status != 202 {
			return fmt.Errorf("open status %d", status)
		}
		var pending struct{ Job string }
		json.Unmarshal(b, &pending)
		var c service.Chunk
		for deadline := time.Now().Add(20 * time.Second); ; {
			status, b, e = request(client, api.URL, "/v1/poll", map[string]any{"v": 1, "job": pending.Job})
			if e != nil {
				return e
			}
			if status == 200 {
				json.Unmarshal(b, &c)
				break
			}
			if status != 202 || time.Now().After(deadline) {
				return fmt.Errorf("poll status %d", status)
			}
			time.Sleep(500 * time.Millisecond)
		}
		firstBytes := len(b)
		totalBytes, textBytes, blocks, anchors, links, forms, images := 0, 0, 0, 0, 0, 0, 0
		tail := false
		maxChunk := 0
		asset := ""
		retained := strings.Builder{}
		for i := 0; i < c.Chunks; i++ {
			status, b, e = request(client, api.URL, "/v1/chunk", map[string]any{"v": 1, "doc": c.Doc, "chunk": i})
			if e != nil || status != 200 {
				return fmt.Errorf("chunk status %d", status)
			}
			totalBytes += len(b)
			maxChunk = max(maxChunk, len(b))
			var part service.Chunk
			json.Unmarshal(b, &part)
			blocks += len(part.Blocks)
			anchors += len(part.Anchors)
			for _, block := range part.Blocks {
				for _, r := range block.Runs {
					textBytes += len(r.Text)
					retained.WriteString(r.Text)
					if r.Href != "" {
						links++
					}
					if strings.Contains(r.Text, "FINAL_ORIGINAL_SENTINEL") {
						tail = true
					}
					if strings.Contains(r.Text, "HIDDEN_SENTINEL") {
						return fmt.Errorf("hidden retained")
					}
				}
				if block.Form != nil {
					forms++
				}
				if block.Image != nil {
					images++
					if asset == "" {
						asset = block.Image.Asset
					}
				}
			}
		}
		outputImage := 0
		if path == "/original" {
			if !tail || c.Truncated {
				return fmt.Errorf("original article tail/truncation failure")
			}
			if asset == "" {
				return fmt.Errorf("image missing")
			}
			status, b, e = request(client, api.URL, "/v1/image", map[string]any{"v": 1, "doc": c.Doc, "asset": asset, "width": 800, "height": 800})
			if e != nil || status != 200 || len(b) < 26 || b[24] != 1 || b[25] != 3 {
				return fmt.Errorf("image status %d", status)
			}
			outputImage = len(b)
		}
		inputBytes := originalHTML
		if path == "/capture" {
			inputBytes = len(capture)
		}
		summary := map[string]any{"source": path, "source_bytes": inputBytes, "chunks": c.Chunks, "first_chunk_bytes": firstBytes, "all_chunk_bytes": totalBytes, "max_chunk_bytes": maxChunk, "text_bytes": textBytes, "blocks": blocks, "anchors": anchors, "linked_runs": links, "forms": forms, "images": images, "image_png_bytes": outputImage, "truncated": c.Truncated, "wall_ms": time.Since(start).Milliseconds(), "retained_text_sha256": fmt.Sprintf("%x", sha256.Sum256([]byte(retained.String())))}
		summaries = append(summaries, summary)
	}
	time.Sleep(6 * time.Second) // respect the real two-open burst before a third admission
	status, b, e := request(client, api.URL, "/v1/open", map[string]any{"v": 1, "rid": strings.Repeat("f", 32), "url": "https://example.com/slow", "mode": "reader", "images": "on-demand"})
	if e != nil || status != 202 {
		return fmt.Errorf("cancel open status %d", status)
	}
	_ = b
	status, _, e = request(client, api.URL, "/v1/cancel", map[string]any{"v": 1, "rid": strings.Repeat("f", 32)})
	if e != nil || status != 200 {
		return fmt.Errorf("cancel status %d", status)
	}
	result := map[string]any{"connection_boundary": "fake net.Pipe; actual HTTP/TLS/extractor/codec/child", "native_arch": runtime.GOARCH, "summaries": summaries, "synthetic_gzip_wire_bytes": compressed.Len(), "synthetic_image_input_bytes": imageData.Len(), "synthetic_image_input_bit_depth": int(imageData.Bytes()[24]), "synthetic_image_input_color_type": int(imageData.Bytes()[25]), "cancel_status": status}
	if len(capture) > 0 {
		result["capture_sha256"] = fmt.Sprintf("%x", sha256.Sum256(capture))
	}
	return json.NewEncoder(os.Stdout).Encode(result)
}
