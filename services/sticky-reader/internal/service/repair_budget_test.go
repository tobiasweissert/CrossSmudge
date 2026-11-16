package service

import (
	"encoding/json"
	"fmt"
	"strings"
	"testing"
)

func TestRepairMODEL001ExactBudgets(t *testing.T) {
	c := Chunk{URL: "https://example.com/", Title: "title", Lang: "en", Dir: "auto", Mode: "simplified", Blocks: []Block{{ID: 1, Kind: "p", Runs: []Run{{Text: "a"}}}}, Anchors: []Anchor{}}
	base := chunkText(c)
	c.Blocks[0].Runs[0].Text = strings.Repeat("a", 8192-base+1)
	if !validChunkBudget(c) {
		t.Fatal("exact decoded boundary rejected")
	}
	c.Blocks[0].Runs[0].Text += "a"
	if validChunkBudget(c) {
		t.Fatal("decoded boundary bypass")
	}
	// Escape expansion consumes the serialized budget before the decoded budget.
	c.Blocks[0].Runs[0].Text = strings.Repeat("<", 7737)
	for i := 0; i < 64; i++ {
		c.Anchors = append(c.Anchors, Anchor{ID: fmt.Sprint(i), Block: 1, Run: 1, Byte: 1})
	}
	c.Chunk = 63
	c.Chunks = 64
	c.Truncated = true
	c.Reason = "too_large"
	for {
		raw, _ := json.Marshal(c)
		if len(raw) >= 49152 {
			if len(raw) != 49152 {
				t.Fatal("boundary construction", len(raw))
			}
			break
		}
		c.Blocks[0].Runs[0].Text += "a"
	}
	if chunkText(c) > 8192 || !validChunkBudget(c) {
		t.Fatal("exact serialized boundary rejected", chunkText(c))
	}
	c.Blocks[0].Runs[0].Text += "a"
	if validChunkBudget(c) {
		t.Fatal("serialized boundary bypass")
	}
}
