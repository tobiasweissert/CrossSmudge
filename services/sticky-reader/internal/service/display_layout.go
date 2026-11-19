package service

import (
	"strconv"
	"strings"

	"golang.org/x/net/html"
	"golang.org/x/net/html/atom"
)

// displayLayout is a bounded simple-selector index, not a CSS engine.
// Only display and literal horizontal margins can materialize text boundaries.
type displayRule struct {
	value  string
	weight int
}
type layoutStyle map[string]displayRule
type displayLayout map[string]layoutStyle

func layoutDeclarations(body string) layoutStyle {
	out := layoutStyle{}
	for _, part := range strings.Split(body, ";") {
		key, v, ok := strings.Cut(part, ":")
		if !ok {
			continue
		}
		key = strings.TrimSpace(strings.ToLower(key))
		v = strings.TrimSpace(strings.ToLower(v))
		priority := strings.HasSuffix(v, "!important")
		v = strings.TrimSpace(strings.TrimSuffix(v, "!important"))
		if len(v) > 80 {
			continue
		}
		weight := 0
		if priority {
			weight = 100000000
		}
		put := func(k, s string) {
			old, exists := out[k]
			if !exists || weight >= old.weight {
				out[k] = displayRule{s, weight}
			}
		}
		switch key {
		case "display", "margin-left", "margin-right":
			put(key, v)
		case "margin":
			a := strings.Fields(v)
			if len(a) < 1 || len(a) > 4 {
				continue
			}
			right, left := a[0], a[0]
			if len(a) > 1 {
				right, left = a[1], a[1]
			}
			if len(a) == 4 {
				left = a[3]
			}
			put("margin-left", left)
			put("margin-right", right)
		}
	}
	return out
}

func (layout displayLayout) collect(rule [2]string, order int) {
	declarations := layoutDeclarations(rule[1])
	if len(declarations) == 0 {
		return
	}
	for _, selector := range strings.Split(rule[0], ",") {
		selector = strings.TrimSpace(selector)
		if selector == "" || len(selector) > 120 || !simpleSelector.MatchString(selector) {
			continue
		}
		weight := order
		if strings.Contains(selector, "#") {
			weight += 10000000
		} else if strings.Contains(selector, ".") {
			weight += 1000000
		}
		if selector[0] != '.' && selector[0] != '#' {
			weight += 100000
		}
		entry, exists := layout[selector]
		if !exists {
			if len(layout) >= 256 {
				continue
			}
			entry = layoutStyle{}
			layout[selector] = entry
		}
		for key, r := range declarations {
			r.weight += weight
			old, ok := entry[key]
			if !ok || r.weight >= old.weight {
				entry[key] = r
			}
		}
	}
}

func (layout displayLayout) style(n *html.Node) layoutStyle {
	best := layoutStyle{}
	pick := func(key string) {
		for k, r := range layout[key] {
			old, ok := best[k]
			if !ok || r.weight >= old.weight {
				best[k] = r
			}
		}
	}
	pick(n.Data)
	if id := attr(n, "id"); id != "" {
		pick("#" + id)
		pick(n.Data + "#" + id)
	}
	for _, class := range strings.Fields(attr(n, "class")) {
		pick("." + class)
		pick(n.Data + "." + class)
	}
	for _, rule := range cssRules("x{" + attr(n, "style") + "}") {
		for k, r := range layoutDeclarations(rule[1]) {
			r.weight += 100000000
			old, ok := best[k]
			if !ok || r.weight >= old.weight {
				best[k] = r
			}
		}
	}
	return best
}

func positiveMargin(v string) bool {
	for _, unit := range []string{"rem", "em", "px"} {
		if strings.HasSuffix(v, unit) {
			number := strings.TrimSuffix(v, unit)
			if number == "" || strings.IndexFunc(number, func(r rune) bool { return (r < '0' || r > '9') && r != '.' }) >= 0 {
				return false
			}
			n, e := strconv.ParseFloat(number, 64)
			return e == nil && n > 0
		}
	}
	return false
}

// Materialize known box/newline and horizontal-space boundaries before
// readability removes class/style attributes. Ordinary inline words are inert.
func (layout displayLayout) separate(n *html.Node) {
	for c := n.FirstChild; c != nil; {
		next := c.NextSibling
		layout.separate(c)
		if c.Type == html.ElementNode {
			style := layout.style(c)
			block := false
			switch style["display"].value {
			case "block", "flex", "grid", "flow-root", "list-item", "table", "-webkit-box":
				block = true
			}
			if style["display"].value != "none" {
				if c.PrevSibling != nil {
					if block && c.PrevSibling.Data != "br" {
						n.InsertBefore(&html.Node{Type: html.ElementNode, Data: "br", DataAtom: atom.Br}, c)
					} else if !block && positiveMargin(style["margin-left"].value) {
						n.InsertBefore(&html.Node{Type: html.TextNode, Data: " "}, c)
					}
				}
				if next != nil {
					if block && next.Data != "br" {
						n.InsertBefore(&html.Node{Type: html.ElementNode, Data: "br", DataAtom: atom.Br}, next)
					} else if !block && positiveMargin(style["margin-right"].value) {
						n.InsertBefore(&html.Node{Type: html.TextNode, Data: " "}, next)
					}
				}
			}
		}
		c = next
	}
}
