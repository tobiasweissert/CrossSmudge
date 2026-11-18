#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include "HttpCore.h"
using namespace ink::http;
struct Peer {
  std::vector<std::string> responses, hosts;
  size_t at = 0;
  int hop = -1;
  static bool connect(void* p, const char* h, uint32_t, const std::atomic<bool>&) {
    auto& s = *(Peer*)p;
    s.hosts.emplace_back(h);
    s.at = 0;
    return size_t(++s.hop) < s.responses.size();
  }
  static int read(void* p, char* b, size_t n) {
    auto& s = *(Peer*)p;
    auto& r = s.responses[s.hop];
    n = std::min({n, r.size() - s.at, size_t(11)});
    memcpy(b, r.data() + s.at, n);
    s.at += n;
    return n;
  }
  static int write(void*, const char*, size_t n) { return n; }
  static void close(void*) {}
  static uint32_t millis(void*) { return 0; }
};
int main() {
  Request q;
  q.browser = true;
  assert(parseUrl("https://example.org/start", q.url));
  std::vector<char> buffer(BrowserBody);
  Result r;
  r.extended = buffer.data();
  r.capacity = buffer.size();
  Scratch s;
  std::atomic<bool> cancel{false};
  auto run = [&](std::vector<std::string> responses) {
    Peer p;
    p.responses = responses;
    Io io{&p, Peer::connect, Peer::read, Peer::write, Peer::close, Peer::millis};
    exchange(q, r, s, io, cancel);
    return p;
  };
  auto p =
      run({"HTTP/1.1 302 Found\r\nLocation: https://news.example.org/story\r\nContent-Length: 0\r\n\r\n",
           "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 12000\r\n\r\n" + std::string(12000, 'x')});
  assert(!r.error && r.size == 12000 && p.hosts.size() == 2 && std::string(r.url) == "https://news.example.org/story");
  assert(std::string(r.contentType) == "text/html");
  run({"HTTP/1.1 302 Found\r\nLocation: https://127.0.0.1/\r\nContent-Length: 0\r\n\r\n"});
  assert(r.error);
  run({"HTTP/1.1 200 OK\r\nContent-Length: 1048577\r\n\r\n"});
  assert(r.error && std::string(r.error) == "body_limit");
  run({"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5;test=yes\r\nhello\r\n0\r\nX-Final: yes\r\n\r\n"});
  assert(!r.error && std::string(r.data(), r.size) == "hello");
  std::vector<std::string> loop(6, "HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n");
  run(loop);
  assert(r.error && std::string(r.error) == "redirect_limit");
  cancel = true;
  run({});
  assert(r.error && std::string(r.error) == "cancelled");
}
