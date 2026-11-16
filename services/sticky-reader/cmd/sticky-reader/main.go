package main

import (
	"context"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/signal"
	"runtime"
	"runtime/debug"
	"syscall"
	"time"

	"sticky-reader/internal/service"
)

func main() {
	if len(os.Args) == 2 && os.Args[1] == "--worker" {
		os.Exit(service.WorkerMain())
	}
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "sticky-reader: startup or shutdown failed")
		os.Exit(1)
	}
}
func run() error {
	runtime.GOMAXPROCS(1)
	debug.SetMemoryLimit(48 << 20)
	verifiers, e := service.LoadVerifiers(os.Getenv("SRP_VERIFIERS"))
	if e != nil {
		return e
	}
	dir := os.Getenv("SRP_CACHE")
	if dir == "" {
		dir = "/cache/session"
	}
	if e = service.ResetCache(dir); e != nil {
		return e
	}
	s, e := service.New(service.Config{CacheDir: dir, Verifiers: verifiers})
	if e != nil {
		return e
	}
	defer s.Close()
	addr := os.Getenv("SRP_LISTEN")
	if addr == "" {
		addr = ":8080"
	}
	h := &http.Server{Addr: addr, Handler: s, ReadHeaderTimeout: 5 * time.Second, ReadTimeout: 5 * time.Second, WriteTimeout: 20 * time.Second, IdleTimeout: 15 * time.Second, MaxHeaderBytes: 8192, ErrorLog: discardLogger()}
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()
	done := make(chan error, 1)
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	limited := service.LimitListener(ln)
	defer limited.Close()
	go func() { done <- h.Serve(limited) }()
	select {
	case e = <-done:
	case <-ctx.Done():
		c, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		e = h.Shutdown(c)
	}
	if e == http.ErrServerClosed {
		return nil
	}
	return e
}
