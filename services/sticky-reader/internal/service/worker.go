package service

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"image"
	"image/color"
	_ "image/jpeg"
	"image/png"
	"io"
	"os"
	"os/exec"
	"runtime/debug"

	"golang.org/x/image/draw"
	_ "golang.org/x/image/webp"
)

func jsonMarshal(v any) ([]byte, error) { return json.Marshal(v) }
func randomID() string {
	var b [16]byte
	if _, e := rand.Read(b[:]); e != nil {
		panic("OS entropy unavailable")
	}
	return hex.EncodeToString(b[:])
}

type Task struct {
	Kind   string   `json:"kind"`
	Data   []byte   `json:"data"`
	URL    string   `json:"url"`
	Mode   string   `json:"mode"`
	CSS    []string `json:"css,omitempty"`
	Width  int      `json:"width"`
	Height int      `json:"height"`
}
type Result struct {
	Document *Document `json:"document,omitempty"`
	PNG      []byte    `json:"png,omitempty"`
	Error    string    `json:"error,omitempty"`
}
type cappedBuffer struct {
	bytes.Buffer
	limit int
}

func (b *cappedBuffer) Write(p []byte) (int, error) {
	if b.Len()+len(p) > b.limit {
		return 0, fail("too_large")
	}
	return b.Buffer.Write(p)
}

// CommandContext kills on expiry; Run always calls Wait before returning. Slot release is the caller's responsibility AFTER this returns.
func runChild(ctx context.Context, t Task, args, extraEnv []string) (Result, error) {
	exe, e := os.Executable()
	if e != nil {
		return Result{}, fail("unreadable")
	}
	if args == nil {
		args = []string{"--worker"}
	}
	input, e := json.Marshal(t)
	if e != nil || len(input) > 8<<20 {
		return Result{}, fail("too_large")
	}
	cmd := exec.CommandContext(ctx, exe, args...)
	cmd.Env = append([]string{"GOMAXPROCS=1", "GOMEMLIMIT=96MiB", "GODEBUG=netdns=go"}, extraEnv...)
	cmd.Stdin = bytes.NewReader(input)
	output := &cappedBuffer{limit: 2 << 20}
	cmd.Stdout = output
	cmd.Stderr = io.Discard
	if e = cmd.Run(); e != nil {
		if ctx.Err() != nil {
			return Result{}, fail("timeout")
		}
		return Result{}, fail("unreadable")
	}
	var result Result
	if e = json.Unmarshal(output.Bytes(), &result); e != nil {
		return Result{}, fail("unreadable")
	}
	if result.Error != "" {
		return result, fail(result.Error)
	}
	return result, nil
}
func WorkerMain() int {
	debug.SetMemoryLimit(96 << 20)
	input, e := io.ReadAll(io.LimitReader(os.Stdin, (8<<20)+1))
	if e != nil || len(input) > 8<<20 {
		return 2
	}
	var t Task
	if e = json.Unmarshal(input, &t); e != nil {
		return 2
	}
	r := Result{}
	switch t.Kind {
	case "html":
		r.Document, e = convertWithCSS(t.Data, t.URL, t.Mode, t.CSS)
	case "image":
		r.PNG, e = convertImage(t.Data, t.Width, t.Height)
	default:
		e = fail("invalid_request")
	}
	if e != nil {
		r.Error = code(e)
	}
	out, e := json.Marshal(r)
	if e != nil || len(out) > 2<<20 {
		return 2
	}
	if _, e = os.Stdout.Write(out); e != nil {
		return 2
	}
	return 0
}
func convertImage(input []byte, w, h int) ([]byte, error) {
	if len(input) > 4<<20 || w < 1 || h < 1 || w > 800 || h > 800 {
		return nil, fail("too_large")
	}
	cfg, format, e := image.DecodeConfig(bytes.NewReader(input))
	if e != nil {
		return nil, fail("unsupported_content")
	}
	if format != "jpeg" && format != "png" && format != "webp" {
		return nil, fail("unsupported_content")
	}
	if cfg.Width < 1 || cfg.Height < 1 || cfg.Width > 4096 || cfg.Height > 4096 || int64(cfg.Width)*int64(cfg.Height) > 4_000_000 {
		return nil, fail("too_large")
	}
	src, actual, e := image.Decode(bytes.NewReader(input))
	if e != nil || actual != format {
		return nil, fail("unsupported_content")
	}
	if src.Bounds().Dx() != cfg.Width || src.Bounds().Dy() != cfg.Height {
		return nil, fail("unsupported_content")
	}
	if w > cfg.Width {
		w = cfg.Width
	}
	if h > cfg.Height {
		h = cfg.Height
	}
	if int64(w)*int64(cfg.Height) <= int64(h)*int64(cfg.Width) {
		h = max(1, w*cfg.Height/cfg.Width)
	} else {
		w = max(1, h*cfg.Width/cfg.Height)
	}
	resized := image.NewNRGBA(image.Rect(0, 0, w, h))
	draw.ApproxBiLinear.Scale(resized, resized.Bounds(), src, src.Bounds(), draw.Src, nil)
	dst := image.NewPaletted(resized.Bounds(), color.Palette{color.Black, color.White})
	row, next := make([]float64, w+2), make([]float64, w+2)
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			r, g, b, a := resized.At(x, y).RGBA()
			white := float64(65535-a) / 257
			v := (0.2126*float64(r)+0.7152*float64(g)+0.0722*float64(b))/257 + white + row[x+1]
			quant := 0.
			if v >= 128 {
				quant = 255
				dst.SetColorIndex(x, y, 1)
			}
			err := v - quant
			row[x+2] += err * 7 / 16
			next[x] += err * 3 / 16
			next[x+1] += err * 5 / 16
			next[x+2] += err / 16
		}
		row, next = next, row
		clear(next)
	}
	out := &cappedBuffer{limit: 65536}
	encoder := png.Encoder{CompressionLevel: png.BestCompression}
	if e = encoder.Encode(out, dst); e != nil {
		return nil, fail("too_large")
	}
	return out.Bytes(), nil
}
