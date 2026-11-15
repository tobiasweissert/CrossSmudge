#pragma once
#include <netdb.h>
#include <netinet/in.h>

#include "PlatformFakes.h"
inline int fakeGetaddrinfo(const char*, const char*, const addrinfo*, addrinfo** out) {
  static sockaddr_in socket{};
  static addrinfo a{};
  socket.sin_addr.s_addr = htonl(fake::address);
  a.ai_family = AF_INET;
  a.ai_addrlen = sizeof(socket);
  a.ai_addr = reinterpret_cast<sockaddr*>(&socket);
  a.ai_next = nullptr;
  *out = &a;
  return 0;
}
inline void fakeFreeaddrinfo(addrinfo*) {}
#define getaddrinfo fakeGetaddrinfo
#define freeaddrinfo fakeFreeaddrinfo
