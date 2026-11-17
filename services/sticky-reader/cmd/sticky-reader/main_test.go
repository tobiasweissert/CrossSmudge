package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"
)

func TestPublicCommandReachesListenWithoutVerifier(t *testing.T) {
	t.Setenv("SRP_VERIFIERS", filepath.Join(t.TempDir(), "must-not-read"))
	dir := filepath.Join(t.TempDir(), "cache")
	t.Setenv("SRP_CACHE", dir)
	// Invalid address proves production startup reached net.Listen without keys.
	t.Setenv("SRP_LISTEN", "127.0.0.1:invalid-port")
	e := run()
	var addr *net.OpError
	if !errors.As(e, &addr) {
		t.Fatalf("expected listener error after keyless startup, got %v", e)
	}
	if _, e = os.Stat(dir); e != nil {
		t.Fatal(e)
	}
}

func TestPublicCommandTCPChild(t *testing.T) {
	if os.Getenv("SRP_PUBLIC_TEST_CHILD") != "1" {
		return
	}
	if e := run(); e != nil {
		fmt.Fprintln(os.Stderr, e)
		os.Exit(1)
	}
	os.Exit(0)
}

// Parent enables kernel-TCP tests on a normal host. Never bypass sandbox policy.
func TestPublicCommandTCP(t *testing.T) {
	if os.Getenv("SRP_TCP_FIXTURES") != "1" {
		t.Skip("set SRP_TCP_FIXTURES=1 for real command TCP")
	}
	ln, e := net.Listen("tcp", "127.0.0.1:0")
	if e != nil {
		t.Fatal(e)
	}
	addr := ln.Addr().String()
	ln.Close()
	t.Setenv("SRP_VERIFIERS", "")
	t.Setenv("SRP_CACHE", filepath.Join(t.TempDir(), "cache"))
	t.Setenv("SRP_LISTEN", addr)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, os.Args[0], "-test.run=^TestPublicCommandTCPChild$")
	cmd.Env = append(os.Environ(), "SRP_PUBLIC_TEST_CHILD=1")
	cmd.Stderr = os.Stderr
	if e = cmd.Start(); e != nil {
		t.Fatal(e)
	}
	defer func() {
		cmd.Process.Signal(syscall.SIGTERM)
		if e := cmd.Wait(); e != nil {
			t.Error(e)
		}
	}()
	client := &http.Client{Timeout: 200 * time.Millisecond}
	defer client.CloseIdleConnections()
	origin := "http://" + addr
	ready := false
	for i := 0; i < 100; i++ {
		r, e := client.Get(origin + "/healthz")
		if e == nil {
			io.Copy(io.Discard, r.Body)
			r.Body.Close()
			if r.StatusCode == 200 {
				ready = true
				break
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	if !ready {
		t.Fatal("public command failed to start without verifier")
	}
	r, e := client.Post(origin+"/v1/cancel", "application/json", strings.NewReader(`{"v":1,"rid":"00000000000000000000000000000000"}`))
	if e != nil {
		t.Fatal(e)
	}
	defer r.Body.Close()
	if r.StatusCode != 200 {
		t.Fatalf("no-auth cancel: %d", r.StatusCode)
	}
}
