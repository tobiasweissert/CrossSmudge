// Integration-only overlay: approved service source bytes remain unchanged.
package service

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestCombinedNativeLua(t *testing.T) {
	out := os.Getenv("COMBINED_OUT")
	if out == "" {
		t.Fatal("COMBINED_OUT required")
	}
	// Existing fixture executes real TLS, vetted public DNS/numeric pin and controlled
	// test CA; SRP_TCP_FIXTURES=1 selects actual kernel loopback for both servers.
	f, _, dials := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Authorization") != "" {
			t.Error("upstream credential leak")
		}
		if r.URL.Path == "/photo.png" {
			w.Header().Set("Content-Type", "image/png")
			im := image.NewNRGBA(image.Rect(0, 0, 128, 64))
			for y := 0; y < 64; y++ {
				for x := 0; x < 128; x++ {
					v := uint8(255)
					if x%2 == 0 {
						v = 0
					}
					im.SetNRGBA(x, y, color.NRGBA{v, v, v, 255})
				}
			}
			png.Encode(w, im)
			return
		}
		w.Header().Set("Content-Type", "text/html")
		io.WriteString(w, `<html lang="en"><title>Combined original</title><article><h1 id="top">Combined original</h1><p id="intro">Beginning café hello, <b>world</b>. </p><p>`+strings.Repeat("Original continued café passage. ", 700)+`</p><p id="end">FINAL_COMBINED_TAIL hello, <b>world</b>.</p><img id="picture" src="/photo.png" width="128" height="64" alt="Original checkerboard"><form id="search" action="/search"><input name="q" type="search"><input name="scope" type="hidden" value="all"></form></article></html>`)
	}))
	s, e := New(Config{CacheDir: t.TempDir(), Fetcher: f, WorkerArgs: []string{"-test.run=^TestWorkerProcess$"}, WorkerEnv: []string{"SRP_TEST_WORKER=1"}})
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	api := fixtureServer(t, s, false)
	defer api.Close()
	// Native client sees complete actual HTTP bytes, through a framed pipe only at
	// its test Io boundary. There is no response/model implementation in this bridge.
	responseRead, responseWrite, e := os.Pipe()
	if e != nil {
		t.Fatal(e)
	}
	requestRead, requestWrite, e := os.Pipe()
	if e != nil {
		t.Fatal(e)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, os.Getenv("COMBINED_NATIVE"), os.Getenv("COMBINED_DRIVER"), "0", os.Getenv("COMBINED_RESOURCES"))
	cmd.Dir = os.Getenv("COMBINED_ROOT")
	cmd.Env = append(os.Environ(), "REPLAY_SERVICE_IPC=1")
	cmd.ExtraFiles = []*os.File{responseRead, requestWrite}
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if e = cmd.Start(); e != nil {
		t.Fatal(e)
	}
	responseRead.Close()
	requestWrite.Close()
	done := make(chan error, 1)
	go func() {
		defer requestRead.Close()
		defer responseWrite.Close()
		for n := 0; ; n++ {
			var length uint32
			if e := binary.Read(requestRead, binary.BigEndian, &length); e != nil {
				if e == io.EOF {
					done <- nil
				} else {
					done <- e
				}
				return
			}
			if length > 8192 {
				done <- fmt.Errorf("native request oversized")
				return
			}
			raw := make([]byte, length)
			if _, e := io.ReadFull(requestRead, raw); e != nil {
				done <- e
				return
			}
			if bytes.Contains(bytes.ToLower(raw), []byte("authorization:")) {
				done <- fmt.Errorf("public native request emitted authorization")
				return
			}
			first, _, _ := bytes.Cut(raw, []byte("\r\n"))
			_, body, _ := bytes.Cut(raw, []byte("\r\n\r\n"))
			trace, _ := json.Marshal(map[string]any{"request_line": string(first), "body": string(body), "public_no_auth": true})
			if e = os.WriteFile(filepath.Join(out, fmt.Sprintf("request-%03d.json", n)), trace, 0600); e != nil {
				done <- e
				return
			}
			// The server's ordinary HTTP parser consumes the native request verbatim.
			conn, e := api.Client().Transport.(*http.Transport).DialContext(context.Background(), "tcp", api.Listener.Addr().String())
			if e != nil {
				done <- e
				return
			}
			if _, e = conn.Write(raw); e != nil {
				conn.Close()
				done <- e
				return
			}
			response, e := io.ReadAll(io.LimitReader(conn, 70001))
			conn.Close()
			if e != nil {
				done <- e
				return
			}
			if len(response) > 70000 {
				done <- fmt.Errorf("response bound")
				return
			}
			if bytes.Contains(raw, []byte("POST /v1/image ")) && bytes.HasPrefix(response, []byte("HTTP/1.1 200")) {
				_, body, ok := bytes.Cut(response, []byte("\r\n\r\n"))
				if !ok {
					done <- fmt.Errorf("PNG framing")
					return
				}
				if e = os.WriteFile(filepath.Join(out, "service-image.png"), body, 0600); e != nil {
					done <- e
					return
				}
			}
			if e = os.WriteFile(filepath.Join(out, fmt.Sprintf("wire-%03d.http", n)), response, 0600); e != nil {
				done <- e
				return
			}
			if e = binary.Write(responseWrite, binary.BigEndian, uint32(len(response))); e != nil {
				done <- e
				return
			}
			if _, e = responseWrite.Write(response); e != nil {
				done <- e
				return
			}
		}
	}()
	err := cmd.Wait()
	if err != nil {
		t.Fatal(err)
	}
	if e = <-done; e != nil {
		t.Fatal(e)
	}
	if dials.Load() < 1 {
		t.Fatal("extractor did not fetch TLS upstream")
	}
	b, _ := json.Marshal(map[string]any{"mode": map[bool]string{true: "kernel TCP", false: "net.Pipe actual HTTP/TLS bytes"}[os.Getenv("SRP_TCP_FIXTURES") == "1"], "controlled": "DNS/public numeric dial mapping/test CA", "upstream_dials": dials.Load(), "native_exit": 0})
	if e = os.WriteFile(filepath.Join(out, "service-result.json"), b, 0600); e != nil {
		t.Fatal(e)
	}
}
