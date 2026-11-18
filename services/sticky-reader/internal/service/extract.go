package service

import (
	"bytes"
	"io"
	"net/url"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"unicode/utf8"

	readability "codeberg.org/readeck/go-readability/v2"
	"golang.org/x/net/html"
)

func attr(n *html.Node, key string) string {
	for _, a := range n.Attr {
		if a.Key == key {
			return a.Val
		}
	}
	return ""
}
func nodeText(n *html.Node) string {
	var b strings.Builder
	var walk func(*html.Node)
	walk = func(x *html.Node) {
		if x.Type == html.TextNode {
			b.WriteString(x.Data)
		}
		for c := x.FirstChild; c != nil; c = c.NextSibling {
			walk(c)
		}
	}
	walk(n)
	return clean(strings.Join(strings.Fields(b.String()), " "), 256<<10)
}
func resolve(base *url.URL, s string) string {
	u, e := base.Parse(strings.TrimSpace(s))
	if e != nil {
		return ""
	}
	u.Fragment = ""
	if u, e = safeURL(u.String()); e != nil {
		return ""
	}
	return u.String()
}
func preflight(data []byte) error {
	if len(data) > 4<<20 {
		return fail("too_large")
	}
	z := html.NewTokenizer(bytes.NewReader(data))
	// Lexical tag balance is not DOM depth: HTML permits implied end tags.
	// Bound parser input here; inspect the actual tree depth after html.Parse.
	count := 0
	for {
		tt := z.Next()
		if tt == html.ErrorToken {
			if z.Err() == io.EOF {
				return nil
			}
			return fail("unreadable")
		}
		count++
		if count > 50000 {
			return fail("too_large")
		}
		if tt == html.StartTagToken || tt == html.SelfClosingTagToken {
			if len(z.Raw()) > 8192 {
				return fail("too_large")
			}
		}
	}
}
func stylesheets(data []byte, base *url.URL) []string {
	z := html.NewTokenizer(bytes.NewReader(data))
	out := []string{}
	for {
		tt := z.Next()
		if tt == html.ErrorToken {
			break
		}
		if tt != html.StartTagToken && tt != html.SelfClosingTagToken {
			continue
		}
		t := z.Token()
		if t.Data != "link" {
			continue
		}
		rel, href := "", ""
		for _, a := range t.Attr {
			if a.Key == "rel" {
				rel = a.Val
			}
			if a.Key == "href" {
				href = a.Val
			}
		}
		if strings.EqualFold(rel, "stylesheet") {
			if s := resolve(base, href); s != "" {
				out = append(out, s)
				if len(out) == 2 {
					break
				}
			}
		}
	}
	return out
}

// cssRules recognizes only complete, unconditional, flat rules. All at-rule
// groups (including screen queries) are intentionally unsupported, not promoted
// to unconditional visibility. One bounded scan; no recursion or CSS execution.
func cssRules(sheet string) [][2]string {
	out := [][2]string{}
	var prelude, body strings.Builder
	depth, rules := 0, 0
	quote := byte(0)
	comment, nested := false, false
	write := func(c byte) {
		if depth == 0 {
			prelude.WriteByte(c)
		} else if depth == 1 {
			body.WriteByte(c)
		}
	}
	for i := 0; i < len(sheet); i++ {
		c := sheet[i]
		if comment {
			if c == '*' && i+1 < len(sheet) && sheet[i+1] == '/' {
				comment = false
				i++
			}
			continue
		}
		if quote != 0 {
			if c == '\\' {
				i++
			} else if c == quote {
				quote = 0
			}
			continue
		}
		if c == '/' && i+1 < len(sheet) && sheet[i+1] == '*' {
			comment = true
			i++
			write(' ')
			continue
		}
		if c == '\'' || c == '"' {
			quote = c
			write('?')
			continue
		}
		// Escaped selector/property syntax is unsupported. Do not interpret its
		// escaped delimiter as a new group boundary or hide declaration.
		if c == '\\' {
			write('?')
			i++
			continue
		}
		switch c {
		case '{':
			rules++
			if rules > 1024 {
				return out
			}
			if depth == 0 {
				body.Reset()
				nested = false
			} else {
				nested = true
			}
			depth++
		case '}':
			if depth == 0 {
				return out
			} // malformed remainder
			depth--
			if depth == 0 {
				s := strings.TrimSpace(prelude.String())
				if !nested && s != "" && s[0] != '@' {
					out = append(out, [2]string{s, body.String()})
				}
				prelude.Reset()
				body.Reset()
			}
		case ';':
			if depth == 0 {
				prelude.Reset()
			} else {
				write(c)
			}
		default:
			write(c)
		}
	}
	return out // incomplete groups, comments and strings never publish a rule
}

var simpleSelector = regexp.MustCompile(`^(?:[a-zA-Z][a-zA-Z0-9-]*)?(?:[.#][a-zA-Z_][a-zA-Z0-9_-]*)?$`)

// HTML class tokens are case-sensitive and separated only by ASCII whitespace.
func hasHTMLClass(classes, want string) bool {
	start := 0
	for i := 0; i <= len(classes); i++ {
		if i == len(classes) || strings.ContainsRune(" \t\n\r\f", rune(classes[i])) {
			if classes[start:i] == want {
				return true
			}
			start = i + 1
		}
	}
	return false
}

func hiddenStyle(s string) bool {
	s = strings.ToLower(strings.ReplaceAll(strings.ReplaceAll(strings.ReplaceAll(s, " ", ""), "\n", ""), "\t", ""))
	return strings.Contains(s, "display:none") || strings.Contains(s, "visibility:hidden") || strings.Contains(s, "content-visibility:hidden")
}
func convertHTML(data []byte, target, mode string) (*Document, error) {
	return convertWithCSS(data, target, mode, nil)
}
func convertWithCSS(data []byte, target, mode string, css []string) (*Document, error) {
	if !utf8.Valid(data) {
		return nil, fail("unsupported_content")
	}
	if e := preflight(data); e != nil {
		return nil, e
	}
	base, e := safeURL(target)
	if e != nil {
		return nil, e
	}
	root, e := html.Parse(bytes.NewReader(data))
	if e != nil {
		return nil, fail("unreadable")
	}
	title, lang, dir := "", "", "auto"
	selectors := []string{}
	fragments := map[string]bool{}
	originalForms := []*html.Node{}
	navigation := []*html.Node{}
	indexCards := []*html.Node{}
	var inspect func(*html.Node, int) error
	nodes := 0
	inspect = func(n *html.Node, depth int) error {
		nodes++
		if depth > 128 || nodes > 50000 {
			return fail("too_large")
		}
		if n.Type == html.ElementNode {
			switch n.Data {
			case "title":
				title = clean(nodeText(n), 200)
			case "html":
				lang = clean(attr(n, "lang"), 35)
				d := attr(n, "dir")
				if d == "ltr" || d == "rtl" {
					dir = d
				}
			case "base":
				if s := resolveLink(base, attr(n, "href")); s != "" {
					base, _ = url.Parse(s)
				}
			case "style":
				s := nodeText(n)
				if len(s) <= 64<<10 {
					css = append(css, s)
				}
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			if e := inspect(c, depth+1); e != nil {
				return e
			}
		}
		return nil
	}
	if e = inspect(root, 0); e != nil {
		return nil, e
	}
	cssBytes := 0
	for _, sheet := range css {
		cssBytes += len(sheet)
		if cssBytes > 128<<10 {
			break
		}
		for _, m := range cssRules(sheet) {
			if hiddenStyle(m[1]) {
				for _, s := range strings.Split(m[0], ",") {
					s = strings.TrimSpace(s)
					if simpleSelector.MatchString(s) && len(selectors) < 256 {
						selectors = append(selectors, s)
					}
				}
			}
		}
	}
	hidden := func(n *html.Node) bool {
		if n.Type != html.ElementNode {
			return false
		}
		switch n.Data {
		case "script", "style", "template", "iframe", "object", "embed", "noscript", "svg", "canvas", "head":
			return true
		}
		if attr(n, "hidden") != "" || hasAttr(n, "hidden") || attr(n, "aria-hidden") == "true" || hiddenStyle(attr(n, "style")) {
			return true
		}
		label := " " + strings.ToLower(attr(n, "class")+" "+attr(n, "id")) + " "
		for _, word := range []string{"advertisement", "advert", "tracking", "tracker", "cookie-banner", "social-share", "paywall", "subscription-wall"} {
			if strings.Contains(label, word) {
				return true
			}
		}
		for _, s := range selectors {
			tag, qual := s, ""
			if i := strings.IndexAny(s, ".#"); i >= 0 {
				tag, qual = s[:i], s[i:]
			}
			if tag != "" && tag != n.Data {
				continue
			}
			if qual == "" {
				return true
			}
			if qual[0] == '#' && attr(n, "id") == qual[1:] {
				return true
			}
			if qual[0] == '.' && hasHTMLClass(attr(n, "class"), qual[1:]) {
				return true
			}
		}
		return false
	}
	var prune func(*html.Node)
	prune = func(n *html.Node) {
		for c := n.FirstChild; c != nil; {
			next := c.NextSibling
			if hidden(c) {
				n.RemoveChild(c)
			} else {
				prune(c)
			}
			c = next
		}
	}
	prune(root)
	var collect func(*html.Node)
	collect = func(n *html.Node) {
		if id := attr(n, "id"); id != "" && len(fragments) < 256 && len(id) <= 256 {
			fragments[clean(id, 256)] = true
		}
		if n.Data == "article" && len(indexCards) < 200 {
			if text := nodeText(n); len(text) >= 20 && len(text) <= 2000 && hasLinkedHeading(n) {
				indexCards = append(indexCards, n)
			}
		}
		if n.Data == "form" {
			originalForms = append(originalForms, n)
		}
		if n.Data == "nav" {
			navigation = append(navigation, n)
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			collect(c)
		}
	}
	collect(root)
	if mode == "reader" {
		article, err := readability.FromDocument(cloneNode(root), base)
		if err != nil || article.Node == nil || len(strings.TrimSpace(nodeText(article.Node))) < 30 {
			return nil, fail("unreadable")
		}
		root = article.Node
		if title == "" {
			title = clean(article.Title(), 200)
		}
		if lang == "" {
			lang = clean(article.Language(), 35)
		}
		// Preserve genuine short index cards the article extractor omitted. This is explicit
		// reader navigation policy, not a switch to simplified extraction.
		retained := strings.Join(strings.Fields(nodeText(root)), "")
		for _, card := range indexCards {
			text := strings.Join(strings.Fields(nodeText(card)), "")
			if text != "" && !strings.Contains(retained, text) {
				root.AppendChild(cloneNode(card))
				retained += text
			}
		}
		for _, n := range navigation {
			root.AppendChild(cloneNode(n))
		}
		for _, n := range originalForms {
			root.AppendChild(cloneNode(n))
		}
	}
	d := &Document{Assets: map[string]string{}}
	blocks := []Block{}
	anchors := map[int][]Anchor{}
	bound := map[string]bool{}
	var marks map[string]int
	total, links, forms, images := 0, 0, 0, 0
	truncated := false
	add := func(b Block, ids []string) {
		size := 0
		for _, r := range b.Runs {
			size += len(r.Text)
		}
		if len(blocks) >= 1024 || total+size > 256<<10 {
			truncated = true
			return
		}
		b.ID = len(blocks) + 1
		for _, r := range b.Runs {
			total += len(r.Text)
		}
		blocks = append(blocks, b)
		_ = ids
	}
	var inline func(*html.Node, Run, *[]Run)
	inline = func(n *html.Node, style Run, runs *[]Run) {
		if id := attr(n, "id"); marks != nil && fragments[id] && !bound[id] {
			offset := 0
			for _, r := range *runs {
				offset += len(r.Text)
			}
			marks[id] = offset
		}

		if n.Type == html.TextNode {
			if len(n.Data) > 256<<10 {
				truncated = true
			}
			text := clean(n.Data, 256<<10)
			if !style.Pre {
				var normalized strings.Builder
				space := len(*runs) == 0
				if !space {
					last := (*runs)[len(*runs)-1].Text
					space = strings.HasSuffix(last, " ") || strings.HasSuffix(last, "\n")
				}
				for _, ch := range n.Data {
					if ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' {
						if !space {
							normalized.WriteByte(' ')
							space = true
						}
					} else {
						normalized.WriteRune(ch)
						space = false
					}
				}
				text = clean(normalized.String(), 256<<10)
			}
			for len(text) > 0 {
				end := len(text)
				if end > 2048 {
					end = 2048
					for !utf8.ValidString(text[:end]) {
						end--
					}
				}
				r := style
				r.Text = text[:end]
				if len(*runs) > 0 {
					last := &(*runs)[len(*runs)-1]
					if last.Bold == r.Bold && last.Size == r.Size && last.Pre == r.Pre && last.Href == r.Href && len(last.Text)+len(r.Text) <= 2048 {
						last.Text += r.Text
					} else {
						*runs = append(*runs, r)
					}
				} else {
					*runs = append(*runs, r)
				}
				text = text[end:]
			}
			return
		}
		if n.Data == "img" || n.Data == "form" {
			return
		}
		if n.Data == "b" || n.Data == "strong" {
			style.Bold = true
		}
		if n.Data == "pre" || n.Data == "code" {
			style.Pre = true
		}
		if n.Data == "a" {
			if s := resolveLink(base, attr(n, "href")); s != "" && links < 200 {
				style.Href = s
				links++
			} else if s != "" {
				truncated = true
			}
		}
		if n.Data == "br" {
			r := style
			r.Text = "\n"
			*runs = append(*runs, r)
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			inline(c, style, runs)
		}
	}
	blockKind := func(tag string) string {
		switch tag {
		case "a", "span", "strong", "b", "em", "code", "td", "th":
			return "p"
		case "p", "h1", "h2", "h3", "li", "pre":
			return tag
		case "h4", "h5", "h6":
			return "h3"
		case "blockquote":
			return "quote"
		case "figcaption":
			return "caption"
		}
		return ""
	}
	var walk func(*html.Node)
	walk = func(n *html.Node) {
		first := len(blocks)
		defer func() {
			id := attr(n, "id")
			if fragments[id] && !bound[id] && len(blocks) > first {
				bid := blocks[first].ID
				anchors[bid] = append(anchors[bid], Anchor{id, bid, 1, 1})
				bound[id] = true
			}
		}()

		if truncated && len(blocks) >= 1024 {
			return
		}
		if n.Data == "img" {
			if images >= 16 {
				truncated = true
				return
			}
			source := resolve(base, attr(n, "src"))
			if source == "" {
				return
			}
			w, _ := strconv.Atoi(attr(n, "width"))
			h, _ := strconv.Atoi(attr(n, "height"))
			if (w > 0 && w < 16) || (h > 0 && h < 16) {
				return
			}
			w = max(1, min(w, 4096))
			h = max(1, min(h, 4096))
			asset := randomID()
			d.Assets[asset] = source
			images++
			add(Block{Kind: "image", Image: &Image{asset, clean(attr(n, "alt"), 200), w, h}}, nil)
			return
		}
		if n.Data == "form" {
			method := strings.ToLower(attr(n, "method"))
			if forms >= 8 {
				truncated = true
				return
			}
			if method != "" && method != "get" {
				return
			}
			action := resolve(base, attr(n, "action"))
			if action == "" {
				return
			}
			f := &Form{Action: action, Fields: []Field{}}
			valid := true
			var fields func(*html.Node)
			fields = func(x *html.Node) {
				if x.Data == "input" {
					typ := strings.ToLower(attr(x, "type"))
					if typ == "" {
						typ = "text"
					}
					if typ == "password" || typ == "file" {
						valid = false
					}
					if typ == "text" || typ == "search" || typ == "hidden" {
						name, val := attr(x, "name"), attr(x, "value")
						if len(name) > 256 || len(val) > 256 || len(f.Fields) >= 12 {
							valid = false
						} else if name != "" {
							f.Fields = append(f.Fields, Field{clean(name, 256), clean(val, 256), typ})
						}
					}
				}
				for c := x.FirstChild; c != nil; c = c.NextSibling {
					fields(c)
				}
			}
			fields(n)
			if valid {
				forms++
				add(Block{Kind: "form", Form: f}, nil)
			}
			return
		}
		kind := blockKind(n.Data)
		if kind != "" {
			runs := []Run{}
			marks = map[string]int{}
			style := Run{}
			if kind == "h1" {
				style.Bold = true
				style.Size = 2
			} else if kind == "h2" || kind == "h3" {
				style.Bold = true
				style.Size = 1
			}
			style.Pre = kind == "pre"
			inline(n, style, &runs)
			if len(runs) > 0 && !runs[len(runs)-1].Pre {
				runs[len(runs)-1].Text = strings.TrimRight(runs[len(runs)-1].Text, " ")
				if runs[len(runs)-1].Text == "" {
					runs = runs[:len(runs)-1]
				}
			}
			if len(runs) > 0 {
				continued := false
				offset := 0
				markIDs := make([]string, 0, len(marks))
				for id := range marks {
					markIDs = append(markIDs, id)
				}
				sort.Strings(markIDs)
				for len(runs) > 0 {
					count, size := 0, 0
					for count < len(runs) && count < 64 && size+len(runs[count].Text)+len(runs[count].Href) <= 4096 {
						size += len(runs[count].Text) + len(runs[count].Href)
						count++
					}
					if count == 0 {
						count = 1
					}
					before := len(blocks)
					add(Block{Kind: kind, Runs: append([]Run(nil), runs[:count]...), Continued: continued}, nil)
					textSize := 0
					for _, r := range runs[:count] {
						textSize += len(r.Text)
					}
					if len(blocks) > before {
						bid := blocks[len(blocks)-1].ID
						for _, id := range markIDs {
							pos := marks[id] - offset
							if bound[id] || pos < 0 || pos >= textSize {
								continue
							}
							ri := 0
							for ri < count && pos >= len(runs[ri].Text) {
								pos -= len(runs[ri].Text)
								ri++
							}
							if ri < count {
								anchors[bid] = append(anchors[bid], Anchor{id, bid, ri + 1, pos + 1})
								bound[id] = true
							}
						}
					}
					offset += textSize
					runs = runs[count:]
					continued = true
				}
			}
			var nested func(*html.Node)
			nested = func(x *html.Node) {
				for c := x.FirstChild; c != nil; c = c.NextSibling {
					if c.Data == "img" || c.Data == "form" {
						walk(c)
					} else {
						nested(c)
					}
				}
			}
			nested(n)
			return
		}
		if n.Type == html.TextNode && strings.TrimSpace(n.Data) != "" {
			marks = nil
			runs := []Run{}
			inline(n, Run{}, &runs)
			for i, r := range runs {
				// Only successors from this single source text node are contiguous.
				add(Block{Kind: "p", Runs: []Run{r}, Continued: i > 0}, nil)
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			walk(c)
		}
	}
	walk(root)
	if len(blocks) == 0 {
		return nil, fail("unreadable")
	}
	prototype := Chunk{V: 1, State: "ready", Doc: strings.Repeat("0", 32), URL: target, Title: title, Lang: lang, Dir: dir, Mode: mode, Blocks: []Block{}, Anchors: []Anchor{}}
	current := prototype
	committedText := 0
	flush := func() {
		current.Chunk = len(d.Chunks)
		committedText += chunkText(current)
		d.Chunks = append(d.Chunks, current)
		current = prototype
		current.Blocks = []Block{}
		current.Anchors = []Anchor{}
	}
	for _, b := range blocks {
		a := []Anchor{}
		sort.Slice(anchors[b.ID], func(i, j int) bool { return anchors[b.ID][i].ID < anchors[b.ID][j].ID })
		for _, anchor := range anchors[b.ID] {
			a = append(a, anchor)
		}
		if len(a) > 64 {
			a = a[:64]
			truncated = true
		}
		candidate := current
		candidate.Blocks = append(append([]Block(nil), current.Blocks...), b)
		candidate.Anchors = append(append([]Anchor(nil), current.Anchors...), a...)
		if !validChunkBudget(candidate) {
			if len(current.Blocks) > 0 {
				flush()
				candidate = current
				candidate.Blocks = []Block{b}
				candidate.Anchors = a
			}
			if !validChunkBudget(candidate) {
				return nil, fail("too_large")
			}
		}
		if committedText+chunkText(candidate) > 256<<10 {
			truncated = true
			break
		}
		if len(d.Chunks) >= 64 {
			truncated = true
			break
		}
		current = candidate
	}
	if len(current.Blocks) > 0 && len(d.Chunks) < 64 {
		flush()
	}
	for i := range d.Chunks {
		d.Chunks[i].Chunks = len(d.Chunks)
		d.Chunks[i].Truncated = truncated
		if truncated {
			d.Chunks[i].Reason = "too_large"
		}
	}
	return d, nil
}
func hasAttr(n *html.Node, key string) bool {
	for _, a := range n.Attr {
		if a.Key == key {
			return true
		}
	}
	return false
}
func cloneNode(n *html.Node) *html.Node {
	c := &html.Node{Type: n.Type, DataAtom: n.DataAtom, Data: n.Data, Namespace: n.Namespace, Attr: append([]html.Attribute(nil), n.Attr...)}
	for x := n.FirstChild; x != nil; x = x.NextSibling {
		c.AppendChild(cloneNode(x))
	}
	return c
}

func resolveLink(base *url.URL, s string) string {
	u, e := base.Parse(strings.TrimSpace(s))
	if e != nil {
		return ""
	}
	fragment := u.Fragment
	u.Fragment = ""
	u.RawFragment = ""
	u, e = safeURL(u.String())
	if e != nil {
		return ""
	}
	for _, r := range fragment {
		if r < 32 || r == 127 {
			return ""
		}
	}
	u.Fragment = fragment
	out := u.String()
	if len(out) > 512 || len(u.RequestURI())+len(u.EscapedFragment()) > 383 {
		return ""
	}
	return out
}

func hasLinkedHeading(n *html.Node) bool {
	heading, link := false, false
	var walk func(*html.Node)
	walk = func(x *html.Node) {
		if x.Data == "h1" || x.Data == "h2" || x.Data == "h3" {
			heading = true
		}
		if x.Data == "a" && attr(x, "href") != "" {
			link = true
		}
		for c := x.FirstChild; c != nil; c = c.NextSibling {
			walk(c)
		}
	}
	walk(n)
	return heading && link
}

// Validate first, appended, and replacement candidates with the largest final
// envelope. Anchor IDs and JSON escaping are part of the same frozen budgets.
func validChunkBudget(c Chunk) bool {
	runs := 0
	hrefs := map[string]bool{}
	for _, b := range c.Blocks {
		runs += len(b.Runs)
		for _, r := range b.Runs {
			if r.Href != "" {
				hrefs[r.Href] = true
			}
		}
	}
	c.Chunk = 63
	c.Chunks = 64
	c.Truncated = true
	c.Reason = "too_large"
	raw, e := jsonMarshal(c)
	return e == nil && len(c.Blocks) <= 64 && len(c.Anchors) <= 64 && runs <= 256 && len(hrefs) <= 64 && chunkText(c) <= 8192 && len(raw) <= 49152
}
