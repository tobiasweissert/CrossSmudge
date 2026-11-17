package service

import (
	"golang.org/x/net/netutil"
	"net"
)

// LimitListener bounds pre-authentication connection/header state as well as the
// handler's smaller body/work admission limit. Excess connections wait in the OS backlog.
func LimitListener(l net.Listener) net.Listener { return netutil.LimitListener(l, 16) }
