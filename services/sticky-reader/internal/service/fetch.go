package service

import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/tls"
	"crypto/x509"
	"golang.org/x/net/idna"
	"io"
	"mime"
	"net"
	"net/http"
	"net/netip"
	"net/url"
	"regexp"
	"strings"
	"time"
)

var hostPattern = regexp.MustCompile(`^[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?)+$`)
var tldPattern = regexp.MustCompile(`^[a-zA-Z]{2,63}$`)

func safeURL(s string) (*url.URL, error) {
	if len(s) > 512 || strings.ContainsAny(s, "\\\x00\t\r\n ") {
		return nil, fail("address_denied")
	}
	for _, c := range s {
		if c < 33 || c > 126 {
			return nil, fail("address_denied")
		}
	}
	u, e := url.Parse(s)
	if e != nil || u.Scheme != "https" || u.User != nil || u.Opaque != "" || u.Fragment != "" || strings.Contains(u.Host, ":") || len(u.Host) > 127 || !hostPattern.MatchString(u.Host) {
		return nil, fail("address_denied")
	}
	host := strings.ToLower(u.Host)
	if normalized, e := idna.Lookup.ToASCII(host); e != nil || normalized != host {
		return nil, fail("address_denied")
	}
	parts := strings.Split(host, ".")
	if !tldPattern.MatchString(parts[len(parts)-1]) {
		return nil, fail("address_denied")
	}
	for _, p := range parts {
		if len(p) > 63 {
			return nil, fail("address_denied")
		}
	}
	for _, suffix := range []string{"localhost", "local", "internal", "test", "invalid", "home", "lan", "nytimes.com", "nyt.com", "nyt.net", "nytimg.com", "nyti.ms", "sticky-browser.tobiasweissert.ch"} {
		if host == suffix || strings.HasSuffix(host, "."+suffix) {
			return nil, fail("address_denied")
		}
	}
	path := u.EscapedPath()
	if u.RawQuery != "" {
		path += "?" + u.RawQuery
	}
	if len(path) > 384 {
		return nil, fail("address_denied")
	}
	unescaped, e := url.PathUnescape(path)
	if e != nil {
		return nil, fail("address_denied")
	}
	for _, c := range unescaped {
		if c < 32 || c == 127 || c == '\\' {
			return nil, fail("address_denied")
		}
	}
	u.Host = host
	if u.Path == "" {
		u.Path = "/"
	}
	return u, nil
}

var deniedPrefixes = []netip.Prefix{}

func init() {
	for _, s := range []string{"46.225.216.130/32", "0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8", "169.254.0.0/16", "172.16.0.0/12", "192.0.0.0/24", "192.0.2.0/24", "192.88.99.0/24", "192.168.0.0/16", "198.18.0.0/15", "198.51.100.0/24", "203.0.113.0/24", "224.0.0.0/4", "240.0.0.0/4", "::/96", "::ffff:0:0/96", "64:ff9b::/96", "64:ff9b:1::/48", "100::/64", "2001::/23", "2001:db8::/32", "2002::/16", "3fff::/20", "fc00::/7", "fe80::/10", "ff00::/8"} {
		deniedPrefixes = append(deniedPrefixes, netip.MustParsePrefix(s))
	}
}
func publicIP(s string) bool {
	a, e := netip.ParseAddr(s)
	if e != nil || !a.IsGlobalUnicast() || a.IsPrivate() || a.Is4In6() {
		return false
	}
	for _, p := range deniedPrefixes {
		if p.Contains(a) {
			return false
		}
	}
	return a.Is4() || netip.MustParsePrefix("2000::/3").Contains(a)
}

type Resolver interface {
	LookupNetIP(context.Context, string, string) ([]netip.Addr, error)
}
type Fetcher struct {
	Resolver Resolver
	Dial     func(context.Context, string, string) (net.Conn, error)
	Roots    *x509.CertPool
}
type budgetKey struct{}
type fetchBudget struct{ wire, decoded int64 }

func boundedFetch(ctx context.Context, wire, decoded int64) context.Context {
	return context.WithValue(ctx, budgetKey{}, &fetchBudget{max(0, wire), max(0, decoded)})
}

type Fetched struct {
	Data    []byte
	URL     string
	MIME    string
	Wire    int64
	Decoded int64
}

func (f *Fetcher) Fetch(ctx context.Context, target, kind string) (Fetched, error) {
	ctx, cancel := context.WithTimeout(ctx, 15*time.Second)
	defer cancel()
	if f == nil {
		f = &Fetcher{}
	}
	res := f.Resolver
	if res == nil {
		res = net.DefaultResolver
	}
	dial := f.Dial
	if dial == nil {
		dial = (&net.Dialer{Timeout: 4 * time.Second}).DialContext
	}
	var result Fetched
	for hop := 0; hop <= 4; hop++ {
		u, e := safeURL(target)
		if e != nil {
			return result, e
		}
		ips, e := res.LookupNetIP(ctx, "ip", u.Hostname())
		if e != nil || len(ips) == 0 || len(ips) > 32 {
			return result, fail("address_denied")
		}
		var selected string
		for _, ip := range ips {
			if !publicIP(ip.String()) {
				return result, fail("address_denied")
			}
			if ip.Is4() && selected == "" {
				selected = ip.String()
			}
		}
		if selected == "" {
			return result, fail("address_denied")
		}
		tr := &http.Transport{Proxy: nil, DisableCompression: true, DisableKeepAlives: true, MaxResponseHeaderBytes: 32768, TLSHandshakeTimeout: 4 * time.Second, ResponseHeaderTimeout: 5 * time.Second, TLSClientConfig: &tls.Config{MinVersion: tls.VersionTLS12, ServerName: u.Hostname(), RootCAs: f.Roots}}
		tr.DialContext = func(c context.Context, network, addr string) (net.Conn, error) {
			conn, e := dial(c, "tcp4", net.JoinHostPort(selected, "443"))
			if e != nil {
				return nil, e
			}
			peer, _, e := net.SplitHostPort(conn.RemoteAddr().String())
			if e != nil || peer != selected {
				conn.Close()
				return nil, fail("address_denied")
			}
			return conn, nil
		}
		client := &http.Client{Transport: tr, CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse }}
		req, _ := http.NewRequestWithContext(ctx, "GET", u.String(), nil)
		req.Header.Set("User-Agent", "StickyReader/1")
		req.Header.Set("Accept-Encoding", "gzip, identity")
		accept := "text/html, application/xhtml+xml"
		if kind == "image" {
			accept = "image/png, image/jpeg, image/webp"
		}
		if kind == "css" {
			accept = "text/css"
		}
		req.Header.Set("Accept", accept)
		resp, e := client.Do(req)
		if e != nil {
			tr.CloseIdleConnections()
			if ctx.Err() != nil {
				return result, fail("timeout")
			}
			if strings.Contains(e.Error(), "tls") || strings.Contains(e.Error(), "certificate") {
				return result, fail("tls_failed")
			}
			return result, fail("upstream_denied")
		}
		headers := 0
		for k, vs := range resp.Header {
			for _, v := range vs {
				headers++
				if len(k)+len(v) > 8192 {
					resp.Body.Close()
					tr.CloseIdleConnections()
					return result, fail("too_large")
				}
			}
		}
		if headers > 100 {
			resp.Body.Close()
			tr.CloseIdleConnections()
			return result, fail("too_large")
		}
		wireLimit := int64(1 << 20)
		decodedLimit := int64(4 << 20)
		if kind == "image" {
			wireLimit = 2 << 20
		}
		if kind == "css" {
			wireLimit = 64 << 10
			decodedLimit = 64 << 10
		}
		if budget, ok := ctx.Value(budgetKey{}).(*fetchBudget); ok {
			wireLimit = min(wireLimit, budget.wire)
			decodedLimit = min(decodedLimit, budget.decoded)
		}
		if resp.ContentLength > wireLimit {
			resp.Body.Close()
			tr.CloseIdleConnections()
			return result, fail("too_large")
		}
		raw, e := io.ReadAll(io.LimitReader(resp.Body, wireLimit+1))
		resp.Body.Close()
		tr.CloseIdleConnections()
		result.Wire += int64(len(raw))
		if budget, ok := ctx.Value(budgetKey{}).(*fetchBudget); ok {
			budget.wire -= int64(len(raw))
		}
		if e != nil {
			if ctx.Err() != nil {
				return result, fail("timeout")
			}
			return result, fail("upstream_denied")
		}
		if int64(len(raw)) > wireLimit || result.Wire > 8<<20 {
			return result, fail("too_large")
		}
		body, e := decodeBody(raw, strings.Join(resp.Header.Values("Content-Encoding"), ","), decodedLimit)
		result.Decoded += int64(len(body))
		if budget, ok := ctx.Value(budgetKey{}).(*fetchBudget); ok {
			budget.decoded -= int64(len(body))
		}
		if e != nil {
			return result, e
		}
		if result.Decoded > 12<<20 {
			return result, fail("too_large")
		}
		trailerBytes := 0
		for k, vs := range resp.Trailer {
			for _, v := range vs {
				trailerBytes += len(k) + len(v)
			}
		}
		if trailerBytes > 4096 {
			return result, fail("too_large")
		}
		if resp.StatusCode >= 300 && resp.StatusCode <= 399 {
			if hop == 4 {
				return result, fail("upstream_denied")
			}
			next, e := u.Parse(resp.Header.Get("Location"))
			if e != nil {
				return result, fail("address_denied")
			}
			next.Fragment = ""
			target = next.String()
			continue
		}
		if resp.StatusCode != 200 {
			return result, fail("upstream_denied")
		}
		mt, _, e := mime.ParseMediaType(resp.Header.Get("Content-Type"))
		if e != nil {
			return result, fail("unsupported_content")
		}
		ok := mt == "text/html" || mt == "application/xhtml+xml"
		if kind == "image" {
			ok = mt == "image/png" || mt == "image/jpeg" || mt == "image/webp"
		}
		if kind == "css" {
			ok = mt == "text/css"
		}
		if !ok {
			return result, fail("unsupported_content")
		}
		result.Data = body
		result.URL = u.String()
		result.MIME = mt
		return result, nil
	}
	return result, fail("upstream_denied")
}
func decodeBody(raw []byte, encoding string, limit int64) ([]byte, error) {
	if encoding == "" || encoding == "identity" {
		if int64(len(raw)) > limit {
			return raw[:min(int64(len(raw)), limit+1)], fail("too_large")
		}
		return raw, nil
	}
	if encoding != "gzip" {
		return nil, fail("unsupported_content")
	}
	r := bytes.NewReader(raw)
	g, e := gzip.NewReader(r)
	if e != nil {
		return nil, fail("unsupported_content")
	}
	g.Multistream(false)
	out, e := io.ReadAll(io.LimitReader(g, limit+1))
	ce := g.Close()
	if int64(len(out)) > limit || len(out) > 65536 && len(out) > len(raw)*40 {
		return out, fail("too_large")
	}
	if e != nil || ce != nil || r.Len() != 0 {
		return out, fail("unsupported_content")
	}
	return out, nil
}
