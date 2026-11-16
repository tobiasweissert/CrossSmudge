package service

import (
	"encoding/hex"
	"io"
	"os"
	"regexp"
)

var principalPattern = regexp.MustCompile(`^[a-zA-Z0-9_-]{1,64}$`)

func LoadVerifiers(path string) (map[string][32]byte, error) {
	if path == "" {
		return nil, fail("unauthorized")
	}
	f, e := os.Open(path)
	if e != nil {
		return nil, fail("unauthorized")
	}
	defer f.Close()
	b, e := io.ReadAll(io.LimitReader(f, 8193))
	if e != nil || len(b) > 8192 {
		return nil, fail("unauthorized")
	}
	var v struct {
		V       int `json:"v"`
		Devices []struct {
			ID  string `json:"id"`
			SHA string `json:"sha256"`
		} `json:"devices"`
	}
	if strictJSON(b, &v) != nil || v.V != 1 || len(v.Devices) < 1 || len(v.Devices) > 16 {
		return nil, fail("unauthorized")
	}
	out := map[string][32]byte{}
	hashes := map[string]bool{}
	for _, d := range v.Devices {
		if !principalPattern.MatchString(d.ID) || !tokenPattern.MatchString(d.SHA) {
			return nil, fail("unauthorized")
		}
		if _, ok := out[d.ID]; ok || hashes[d.SHA] {
			return nil, fail("unauthorized")
		}
		raw, _ := hex.DecodeString(d.SHA)
		var hash [32]byte
		copy(hash[:], raw)
		out[d.ID] = hash
		hashes[d.SHA] = true
	}
	return out, nil
}
