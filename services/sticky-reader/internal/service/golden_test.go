package service

import (
	"encoding/json"
	"os"
	"reflect"
	"strings"
	"testing"
)

func TestCanonicalWireGolden(t *testing.T) {
	source, e := os.ReadFile("../../testdata/original.html")
	if e != nil {
		t.Fatal(e)
	}
	d, e := convertHTML(source, "https://example.org/story", "simplified")
	if e != nil {
		t.Fatal(e)
	}
	if len(d.Chunks) != 1 {
		t.Fatal("golden chunk count")
	}
	got := d.Chunks[0]
	got.Doc = strings.Repeat("a", 32)
	for i := range got.Blocks {
		if got.Blocks[i].Image != nil {
			got.Blocks[i].Image.Asset = strings.Repeat("b", 32)
		}
	}
	raw, e := os.ReadFile("../../testdata/ready.json")
	if e != nil {
		t.Fatal(e)
	}
	var want Chunk
	if e = strictJSON(raw, &want); e != nil {
		t.Fatal(e)
	}
	if !reflect.DeepEqual(got, want) {
		g, _ := json.Marshal(got)
		t.Fatalf("canonical synthetic wire changed: %s", g)
	}
}
