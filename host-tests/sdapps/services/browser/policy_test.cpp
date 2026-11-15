#include "BrowserPolicy.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace stickyweb;
  Destination d;
  assert(parseDestination("https://example.org/read?q=1#top", d));
  assert(d.host == "example.org" && d.target == "/read?q=1");
  for (const char* url : {"http://example.org", "https://user@example.org/", "https://127.0.0.1/", "https://[::1]/", "https://example.org:444/", "https://example.org/\r\nX:yes", "https://example.org\\@evil.test/", "https://localhost/", "https://example.local/", "https://example.org/%0d%0aevil"})
    assert(!parseDestination(url, d));
  assert(parseDestination("https://EXAMPLE.org:443", d));
  assert(d.host == "example.org" && d.target == "/");
  for(const char* url : {"https://example.org/%", "https://example.org/%a", "https://example.org/%zz", "https://example.org/%5cpath"}) assert(!parseDestination(url,d));
  assert(publicIpv4(0x08080808));
  for (uint32_t ip : {0x00010203u,0x0a000001u,0x64400001u,0x7f000001u,0xa9fe0001u,0xac100001u,0xc0000001u,0xc0000201u,0xc0a80101u,0xc6120001u,0xc6336401u,0xcb007101u,0xe0000001u,0xffffffffu}) assert(!publicIpv4(ip));
  std::cout << "URL admission and public IPv4 tests passed\n";
}
