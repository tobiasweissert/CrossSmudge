// Package service implements the private SRP1 conversion service.
package service

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"regexp"
	"strconv"
	"strings"
	"unicode/utf8"
)

const Origin = "https://sticky.tobiasweissert.ch"

var idPattern = regexp.MustCompile(`^[0-9a-f]{32}$`)
var tokenPattern = regexp.MustCompile(`^[0-9a-f]{64}$`)

type fault string

func (f fault) Error() string { return string(f) }
func fail(s string) error     { return fault(s) }
func code(e error) string {
	var f fault
	if errors.As(e, &f) {
		return string(f)
	}
	return "unreadable"
}

type Run struct {
	Text string `json:"text"`
	Size int    `json:"size,omitempty"`
	Bold bool   `json:"bold,omitempty"`
	Pre  bool   `json:"pre,omitempty"`
	Href string `json:"href,omitempty"`
}
type Image struct {
	Asset  string `json:"asset"`
	Alt    string `json:"alt"`
	Width  int    `json:"width"`
	Height int    `json:"height"`
}
type Field struct {
	Name  string `json:"name"`
	Value string `json:"value"`
	Type  string `json:"type"`
}
type Form struct {
	Action string  `json:"action"`
	Fields []Field `json:"fields"`
}
type Block struct {
	ID        int    `json:"id"`
	Kind      string `json:"kind"`
	Runs      []Run  `json:"runs,omitempty"`
	Continued bool   `json:"continued,omitempty"`
	Image     *Image `json:"image,omitempty"`
	Form      *Form  `json:"form,omitempty"`
}
type Anchor struct {
	ID    string `json:"id"`
	Block int    `json:"block"`
	Run   int    `json:"run"`
	Byte  int    `json:"byte"`
}
type Chunk struct {
	V         int      `json:"v"`
	State     string   `json:"state"`
	Doc       string   `json:"doc"`
	URL       string   `json:"url"`
	Title     string   `json:"title"`
	Lang      string   `json:"lang"`
	Dir       string   `json:"dir"`
	Mode      string   `json:"mode"`
	Chunks    int      `json:"chunks"`
	Chunk     int      `json:"chunk"`
	Truncated bool     `json:"truncated"`
	Reason    string   `json:"reason,omitempty"`
	Blocks    []Block  `json:"blocks"`
	Anchors   []Anchor `json:"anchors"`
}
type Document struct {
	Chunks  []Chunk           `json:"chunks"`
	Assets  map[string]string `json:"assets"`
	Wire    int64             `json:"wire"`
	Decoded int64             `json:"decoded"`
}
type Position struct {
	Chunk int `json:"chunk"`
	Block int `json:"block"`
	Run   int `json:"run"`
	Byte  int `json:"byte"`
}
type Found struct {
	V     int  `json:"v"`
	Found bool `json:"found"`
	Chunk *int `json:"chunk,omitempty"`
	Block int  `json:"block,omitempty"`
	Run   int  `json:"run,omitempty"`
	Byte  int  `json:"byte,omitempty"`
}

func found(p Position) Found { return Found{1, true, &p.Chunk, p.Block, p.Run, p.Byte} }
func clean(s string, n int) string {
	s = strings.Map(func(r rune) rune {
		if r == 0 || r < 32 && r != '\n' && r != '\t' || r == 127 {
			return -1
		}
		return r
	}, strings.ToValidUTF8(s, ""))
	if len(s) > n {
		s = s[:n]
		for !utf8.ValidString(s) {
			s = s[:len(s)-1]
		}
	}
	return s
}
func chunkText(c Chunk) int {
	n := len(c.Title) + len(c.Lang) + len(c.URL) + len(c.Dir) + len(c.Mode)
	for _, b := range c.Blocks {
		for _, r := range b.Runs {
			n += len(r.Text) + len(r.Href)
		}
		if b.Image != nil {
			n += len(b.Image.Alt)
		}
		if b.Form != nil {
			n += len(b.Form.Action)
			for _, f := range b.Form.Fields {
				n += len(f.Name) + len(f.Value) + len(f.Type)
			}
		}
	}
	for _, a := range c.Anchors {
		n += len(a.ID)
	}
	return n
}

// encoding/json deliberately accepts invalid surrogate escapes. Reject those before decoding.
func strictJSON(b []byte, dst any) error {
	if !utf8.Valid(b) {
		return fail("invalid_request")
	}
	for i := 0; i < len(b); i++ {
		if b[i] != '"' {
			continue
		}
		i++
		for i < len(b) && b[i] != '"' {
			if b[i] < 32 {
				return fail("invalid_request")
			}
			if b[i] == '\\' {
				i++
				if i >= len(b) {
					return fail("invalid_request")
				}
				if b[i] == 'u' {
					if i+4 >= len(b) {
						return fail("invalid_request")
					}
					v, e := strconv.ParseUint(string(b[i+1:i+5]), 16, 16)
					if e != nil {
						return fail("invalid_request")
					}
					i += 4
					if v >= 0xd800 && v <= 0xdbff {
						if i+6 >= len(b) || string(b[i+1:i+3]) != "\\u" {
							return fail("invalid_request")
						}
						w, e := strconv.ParseUint(string(b[i+3:i+7]), 16, 16)
						if e != nil || w < 0xdc00 || w > 0xdfff {
							return fail("invalid_request")
						}
						i += 6
					} else if v >= 0xdc00 && v <= 0xdfff {
						return fail("invalid_request")
					}
				}
			}
			i++
		}
	}
	dec := json.NewDecoder(bytes.NewReader(b))
	dec.UseNumber()
	var walk func(int) error
	walk = func(depth int) error {
		if depth > 12 {
			return fail("invalid_request")
		}
		t, e := dec.Token()
		if e != nil {
			return e
		}
		switch v := t.(type) {
		case json.Delim:
			if v == '{' {
				seen := map[string]bool{}
				for dec.More() {
					k, e := dec.Token()
					if e != nil {
						return e
					}
					s, ok := k.(string)
					if !ok || seen[s] {
						return fail("invalid_request")
					}
					seen[s] = true
					if e = walk(depth + 1); e != nil {
						return e
					}
				}
				t, e = dec.Token()
				if e != nil || t != json.Delim('}') {
					return fail("invalid_request")
				}
			} else if v == '[' {
				for dec.More() {
					if e = walk(depth + 1); e != nil {
						return e
					}
				}
				t, e = dec.Token()
				if e != nil || t != json.Delim(']') {
					return fail("invalid_request")
				}
			} else {
				return fail("invalid_request")
			}
		case string:
			for _, r := range v {
				if r == 0 || r < 32 && r != '\n' && r != '\t' || r == 127 {
					return fail("invalid_request")
				}
			}
		case json.Number:
			n, e := strconv.ParseInt(string(v), 10, 32)
			if e != nil || n > 999999 || n < 0 {
				return fail("invalid_request")
			}
		}
		return nil
	}
	if e := walk(0); e != nil {
		return fail("invalid_request")
	}
	if _, e := dec.Token(); e != io.EOF {
		return fail("invalid_request")
	}
	d := json.NewDecoder(bytes.NewReader(b))
	d.DisallowUnknownFields()
	if e := d.Decode(dst); e != nil {
		return fail("invalid_request")
	}
	return nil
}
