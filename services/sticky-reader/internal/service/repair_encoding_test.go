package service

import (
	"bytes"
	"compress/gzip"
	"context"
	"net/http"
	"testing"
)

func TestRepairEncodingGzipList(t *testing.T) {
	var encoded bytes.Buffer
	g := gzip.NewWriter(&encoded)
	g.Write([]byte("<p>synthetic gzip</p>"))
	g.Close()
	for _, fields := range [][]string{{"gzip", "br"}, {"gzip", "gzip"}, {"gzip, gzip"}, {"gzip"}} {
		f, _, _ := fixtureFetcher(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "text/html")
			for _, v := range fields {
				w.Header().Add("Content-Encoding", v)
			}
			w.Write(encoded.Bytes())
		}))
		out, e := f.Fetch(context.Background(), "https://example.com/", "html")
		if len(fields) == 1 && fields[0] == "gzip" {
			if e != nil || string(out.Data) != "<p>synthetic gzip</p>" {
				t.Fatal("single gzip rejected", e)
			}
		} else if code(e) != "unsupported_content" {
			t.Errorf("stacked gzip list %q accepted: %v", fields, e)
		}
	}
}
