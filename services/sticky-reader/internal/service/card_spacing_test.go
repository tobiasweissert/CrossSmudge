package service

import (
	"strings"
	"testing"
)

func TestLiteralMarginCascade(t *testing.T) {
	for _, c := range []struct {
		css         string
		left, right bool
	}{
		{"margin:0 2px", true, true}, {"margin:0 .2em 0 0", false, true},
		{"margin:0 0 0 .25rem", true, false}, {"margin-left:2px;margin:0", false, false},
		{"margin:0 2px;margin-left:0;margin-right:0", false, false},
		{"margin-left:2px!important;margin:0", true, false},
		{"margin:0 auto", false, false}, {"margin:0 -2px", false, false},
		{"margin:0 2%", false, false}, {"margin:0 calc(2px)", false, false},
		{"margin:0 var(--space)", false, false}, {"margin:0 0", false, false},
	} {
		t.Run(c.css, func(t *testing.T) {
			s := layoutDeclarations(c.css)
			if positiveMargin(s["margin-left"].value) != c.left || positiveMargin(s["margin-right"].value) != c.right {
				t.Fatal("literal margin/cascade mismatch")
			}
		})
	}
}

func TestMetadataMarginBoundaries(t *testing.T) {
	source := `<html><head><title>Metadata</title><style>/*` + strings.Repeat("x", 30000) + `*/.stamp{margin:0 0.25rem}</style></head><body><article><p><span>Updated</span><time class="stamp" id="stamp">18 November</time><span>today</span></p><p>co<strong>operate</strong>! A<time>pril</time>.</p><p>` + strings.Repeat("Original explanatory article passage. ", 20) + `</p></article></body></html>`
	for _, mode := range []string{"reader", "simplified"} {
		t.Run(mode, func(t *testing.T) {
			d, e := convertHTML([]byte(source), "https://example.org/", mode)
			if e != nil {
				t.Fatal(e)
			}
			var text strings.Builder
			for _, c := range d.Chunks {
				for _, b := range c.Blocks {
					for _, r := range b.Runs {
						text.WriteString(r.Text)
					}
					text.WriteByte('\n')
				}
			}
			if !strings.Contains(text.String(), "Updated 18 November today") {
				t.Fatal("horizontal CSS margins joined")
			}
			if !strings.Contains(text.String(), "cooperate! April.") {
				t.Fatal("unspaced inline text changed")
			}
			if !find(d, "18 November", nil).Found {
				t.Fatal("Find lost date")
			}
		})
	}
}

func TestCardDisplayBoundaries(t *testing.T) {
	source := `<html><head><title>Original cards</title><style>.label{display:flex}.headline{display:-webkit-box}.inline{display:inline-block}</style></head><body><main><article><a href="/story"><h2><span class="label"><span>Village AB</span></span><span class="headline" id="headline">Quoted original headline</span></h2></a><p>co<strong>operate</strong>, mid<a href="/word">word</a>! <span class="inline">Un</span><span>broken</span>.</p><p>` + strings.Repeat("Original explanatory article passage. ", 20) + `</p></article></main></body></html>`
	for _, large := range []bool{false, true} {
		htmlSource := source
		if large {
			htmlSource = strings.Replace(source, "<style>", "<style>/*"+strings.Repeat("x", 70000)+"*/", 1)
		}
		for _, mode := range []string{"reader", "simplified"} {
			t.Run(mode+map[bool]string{false: "-small", true: "-large"}[large], func(t *testing.T) {
				d, e := convertHTML([]byte(htmlSource), "https://example.org/", mode)
				if e != nil {
					t.Fatal(e)
				}
				var text strings.Builder
				for _, c := range d.Chunks {
					for _, b := range c.Blocks {
						for _, r := range b.Runs {
							text.WriteString(r.Text)
						}
						text.WriteByte('\n')
					}
				}
				s := text.String()
				if !strings.Contains(s, "Village AB\nQuoted original headline") {
					t.Fatalf("CSS card boxes joined: %q", s)
				}
				if !strings.Contains(s, "cooperate, midword! Unbroken.") {
					t.Fatalf("inline words changed: %q", s)
				}
				found := find(d, "Quoted", nil)
				if !found.Found {
					t.Fatal("Find lost headline")
				}
				anchored := false
				for _, chunk := range d.Chunks {
					for _, a := range chunk.Anchors {
						if a.ID == "headline" {
							anchored = true
							if a.Block != found.Block || a.Run != found.Run || a.Byte != found.Byte {
								t.Fatal("headline byte anchor differs from Find")
							}
						}
					}
				}
				if !anchored {
					t.Fatal("headline fragment disappeared")
				}
			})
		}
	}
}
