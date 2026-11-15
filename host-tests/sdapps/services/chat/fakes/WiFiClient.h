#pragma once
#include <cstdint>

#include "PlatformFakes.h"
struct IPAddress {
  uint32_t value;
  IPAddress(unsigned a, unsigned b, unsigned c, unsigned d) : value((a << 24) | (b << 16) | (c << 8) | d) {}
};
class WiFiClient {
  bool open = false;

 public:
  void setConnectionTimeout(unsigned) {}
  void setTimeout(unsigned) {}
  bool connect(IPAddress ip, unsigned port) {
    if (!fake::connect || ip.value != fake::address || port != 443) return false;
    open = true;
    ++fake::tcpOpen;
    return true;
  }
  void stop() {
    if (open) --fake::tcpOpen;
    open = false;
  }
  bool connected() { return open; }
  int available() { return 0; }
  int read(uint8_t*, int) { return 0; }
  int write(const uint8_t*, int n) { return n; }
};
