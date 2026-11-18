package service

import (
	"strings"
	"testing"
)

// Exercise the same inline and fetched-sheet injection seams in both modes.
func reviewCSS(t *testing.T, sheet, classes string, hidden bool) {
	t.Helper()
	for _, mode := range []string{"simplified", "reader"} {
		for _, external := range []bool{false, true} {
			t.Run(mode+map[bool]string{false: "-inline", true: "-fetched"}[external], func(t *testing.T) {
				source := "<html><head><title>Synthetic original article</title></head><body><article><h1>Original article</h1>" + strings.Repeat("<p>VISIBLE original story, with enough meaningful text to retain the full synthetic article for reader selection.</p>", 4) + "<p class=\"" + classes + "\">MARKER original paragraph, with sufficient text to retain it when it is actually visible on screen.</p></article></body></html>"
				var sheets []string
				if external {
					sheets = []string{sheet}
				} else {
					source = strings.Replace(source, "</head>", "<style>"+sheet+"</style></head>", 1)
				}
				d, e := convertWithCSS([]byte(source), "https://example.org/", mode, sheets)
				if e != nil {
					t.Fatal(e)
				}
				if !strings.Contains(repairText(d), "VISIBLE") {
					t.Fatal("visible positive control lost")
				}
				if strings.Contains(repairText(d), "MARKER") == hidden {
					t.Fatalf("wrong marker visibility; expected hidden=%v", hidden)
				}
			})
		}
	}
}
func TestReviewCSSGroupScope(t *testing.T) {
	for name, sheet := range map[string]string{
		"print":              "@media print{.secret{display:none}}",
		"nested":             "@media screen{@supports(display:grid){.secret{display:none}}}",
		"unknown":            "@unknown x{.secret{display:none}}",
		"quoted-group":       "@media print{p{content:\"}\\\"{\"}.secret{display:none}}",
		"comment-group":      "@media print{/* } */.secret{display:none}}",
		"comment-rule":       "/* .secret{display:none} */p{color:black}",
		"quoted-declaration": ".secret{content:\"display:none } {\"}",
		"unclosed":           "@media print{.secret{display:none}",
		"unclosed-string":    "@media print{p{content:\"} .secret{display:none}}",
		"unclosed-comment":   "/* .secret{display:none}",
		"deep-group":         "@media print{" + strings.Repeat("x{", 1500) + ".secret{display:none}" + strings.Repeat("}", 1501),
		"rule-budget":        strings.Repeat("span{color:black}", 1024) + ".secret{display:none}",
		"byte-budget":        "/*" + strings.Repeat("x", 128<<10) + "*/.secret{display:none}",
	} {
		t.Run(name, func(t *testing.T) { reviewCSS(t, sheet, "secret", false) })
	}
	for name, sheet := range map[string]string{
		"simple":          ".secret{display:none}",
		"after-group":     "@media print{.other{display:none}}.secret{display:none}",
		"after-semicolon": "@charset \"UTF-8\";.secret{display:none}",
		"quoted-braces":   "p{content:\"}\\\"{\"}.secret{display:none}",
		"comment-braces":  "/* } { */.secret{/* } */display:none}",
	} {
		t.Run(name, func(t *testing.T) { reviewCSS(t, sheet, "secret", true) })
	}
}

func TestReviewCSSClassWhitespace(t *testing.T) {
	for _, sep := range []string{" ", "\t", "\n", "\r", "\f", " \t\r\n\f "} {
		reviewCSS(t, ".secret{display:none}", "other"+sep+"secret", true)
		reviewCSS(t, "p.secret{display:none}", "other"+sep+"secret", true)
	}
	for _, classes := range []string{"secretary", "Secret", "other", "other\u00a0secret", "other\vsecret"} {
		d, e := convertHTML([]byte("<style>.secret{display:none}</style><p class=\""+classes+"\">MARKER visible</p>"), "https://example.org/", "simplified")
		if e != nil || !strings.Contains(repairText(d), "MARKER") {
			t.Fatalf("nonmatching exact class hidden: %q %v", classes, e)
		}
	}
}

func TestReviewBareTextChunkBoundary(t *testing.T) {
	for _, tag := range []string{"div", "p"} {
		d, e := convertHTML([]byte("<"+tag+">"+strings.Repeat("x", 12000)+"</"+tag+">"), "https://example.org/", "simplified")
		if e != nil {
			t.Fatal(e)
		}
		boundary := 0
		for _, b := range d.Chunks[0].Blocks {
			for _, r := range b.Runs {
				boundary += len(r.Text)
			}
		}
		prefix := boundary - 3
		d, e = convertHTML([]byte("<"+tag+">"+strings.Repeat("x", prefix)+"éNEEDLE"+strings.Repeat("y", 12000-prefix-8)+"</"+tag+">"), "https://example.org/", "simplified")
		if e != nil {
			t.Fatal(e)
		}
		actualBoundary := 0
		for _, b := range d.Chunks[0].Blocks {
			for _, r := range b.Runs {
				actualBoundary += len(r.Text)
			}
		}
		if !(prefix < actualBoundary && prefix+len("éNEEDLE") > actualBoundary) {
			t.Fatal("fixture match does not cross actual chunk boundary")
		}
		f := find(d, "éNEEDLE", nil)
		if !f.Found || *f.Chunk != 0 || !validPosition(d, Position{*f.Chunk, f.Block, f.Run, f.Byte}) {
			t.Fatalf("missing/invalid cross-chunk match: %+v", f)
		}
	}
}

func TestReviewBareTextContinuationFind(t *testing.T) {
	for _, tag := range []string{"div", "p"} {
		for _, prefix := range []int{2045, 8189} {
			t.Run(tag+strings.Repeat("x", prefix/2048), func(t *testing.T) {
				source := "<" + tag + ">" + strings.Repeat("x", prefix) + "éNEEDLE" + strings.Repeat("y", 2040) + "éNEEDLE tail</" + tag + ">"
				d, e := convertHTML([]byte(source), "https://example.org/", "simplified")
				if e != nil {
					t.Fatal(e)
				}
				if !strings.Contains(repairText(d), "éNEEDLE") || d.Chunks[0].Truncated {
					t.Fatal("fixture text lost")
				}
				first := find(d, "éNEEDLE", nil)
				if !first.Found {
					t.Fatal("contiguous bare text split not found")
				}
				pos := Position{*first.Chunk, first.Block, first.Run, first.Byte}
				if !validPosition(d, pos) {
					t.Fatalf("invalid UTF8 cursor: %+v", pos)
				}
				offset := 0
				for ci, c := range d.Chunks {
					for _, b := range c.Blocks {
						for ri, r := range b.Runs {
							if ci == pos.Chunk && b.ID == pos.Block && ri+1 == pos.Run {
								if offset+pos.Byte-1 != prefix {
									t.Fatalf("wrong UTF8 start offset: %+v offset=%d", pos, offset)
								}
							}
							offset += len(r.Text)
						}
					}
				}
				second := find(d, "éNEEDLE", &pos)
				if !second.Found {
					t.Fatal("Find-next missed retained successor")
				}
				next := Position{*second.Chunk, second.Block, second.Run, second.Byte}
				if !validPosition(d, next) {
					t.Fatal("invalid next cursor")
				}
				if find(d, "éNEEDLE", &next).Found {
					t.Fatal("exclusive Find-next repeated match")
				}
				if prefix > 8192-10 && len(d.Chunks) < 2 {
					t.Fatal("chunk-crossing fixture did not cross chunks")
				}
			})
		}
	}
	for _, source := range []string{"<div>NEE</div><div>DLE tail</div>", "<p>NEE</p><p>DLE tail</p>", "<div>NEE<span>DLE</span></div>"} {
		d, e := convertHTML([]byte(source), "https://example.org/", "simplified")
		if e != nil {
			t.Fatal(e)
		}
		if find(d, "NEEDLE", nil).Found {
			t.Fatal("fabricated cross-source-node match")
		}
	}
}

// CC-GO-001: HTML's tree builder, not lexical tag balance, owns depth.
func TestReviewOptionalEndTags(t *testing.T) {
	for _, tag := range []string{"p", "li", "dt", "dd", "tr"} {
		for _, close := range []bool{false, true} {
			t.Run(tag+map[bool]string{true: "-explicit", false: "-optional"}[close], func(t *testing.T) {
				var body strings.Builder
				for i := 0; i < 140; i++ {
					body.WriteString("<" + tag + ">Original sibling text.")
					if close {
						body.WriteString("</" + tag + ">")
					}
				}
				if tag == "li" {
					b := body.String()
					body.Reset()
					body.WriteString("<ul>" + b + "</ul>")
				}
				if tag == "tr" {
					b := strings.ReplaceAll(body.String(), "Original sibling text.", "<td>Original sibling text.")
					body.Reset()
					body.WriteString("<table>" + b + "</table>")
				}
				for _, mode := range []string{"simplified", "reader"} {
					d, e := convertHTML([]byte("<html><body>"+body.String()+"</body></html>"), "https://example.org/", mode)
					if e != nil || d == nil {
						t.Fatalf("shallow valid siblings rejected (%s): %v", mode, e)
					}
				}
			})
		}
	}
}

func TestReviewHTMLResourceBounds(t *testing.T) {
	// Exactly 50000 tokens pass preflight but implied html/head/body nodes
	// push the actual tree beyond 50000 nodes. Keep both independent gates.
	nodeLimit := strings.Repeat("<br>", 49999) + "original"
	if e := preflight([]byte(nodeLimit)); e != nil {
		t.Fatalf("node-only fixture rejected preparse: %v", e)
	}
	if d, e := convertHTML([]byte(nodeLimit), "https://example.org/", "simplified"); e == nil || d != nil {
		t.Fatal("actual tree node limit lost")
	}

	for name, body := range map[string]string{
		"bytes":               strings.Repeat("x", (4<<20)+1),
		"tokens":              strings.Repeat("<br>", 50001),
		"tag":                 "<p title=\"" + strings.Repeat("x", 8192) + "\">text</p>",
		"depth":               strings.Repeat("<div>", 140) + "original" + strings.Repeat("</div>", 140),
		"unmatched-end-depth": strings.Repeat("<div></span>", 140) + "original" + strings.Repeat("</div>", 140),
	} {
		t.Run(name, func(t *testing.T) {
			if d, e := convertHTML([]byte(body), "https://example.org/", "simplified"); e == nil || d != nil {
				t.Fatal("oversized resource accepted")
			}
		})
	}
}
