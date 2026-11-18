#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/HttpRequest.h"
using namespace airdrop;
static SinkResult accept(void* p, const uint8_t*, size_t n) {
  *static_cast<uint64_t*>(p) += n;
  return SinkResult::Consumed;
}
static BodyState feed(BodyDecoder& d, const std::string& s, uint64_t& payload, bool fragmented) {
  BodyState state = BodyState::NeedMore;
  for (size_t at = 0; at < s.size();) {
    size_t used = 0;
    state =
        d.feed(reinterpret_cast<const uint8_t*>(s.data() + at), fragmented ? 1 : s.size() - at, used, accept, &payload);
    at += used;
    if (state == BodyState::Error || state == BodyState::Complete) return state;
    assert(used);
  }
  return state;
}
int main() {
  for (bool fragmented : {false, true})
    for (unsigned count : {16383u, 16384u, 16385u}) {
      HttpRequest r{};
      r.framing = Framing::Chunked;
      BodyDecoder d(r, 40000000, 42000000);
      uint64_t payload = 0;
      BodyState state{};
      for (unsigned i = 0; i < count; ++i) {
        state = feed(d, "80\r\n" + std::string(128, 'x') + "\r\n", payload, fragmented);
        if (state == BodyState::Error) break;
      }
      if (state != BodyState::Error) state = feed(d, "0\r\n\r\n", payload, fragmented);
      if ((state == BodyState::Complete) != (count <= 16384)) {
        std::fprintf(stderr, "FAIL terminal zero must not spend data-chunk budget count=%u fragmented=%d\n", count,
                     fragmented);
        return 1;
      }
      assert(payload == uint64_t(count <= 16384 ? count : 16384) * 128);
    }
  for (const char* ending : {"0\r\nX\r\n", "0\r\n\rX", "0\n\n", "0;bad\r\n\r\n"}) {
    HttpRequest r{};
    r.framing = Framing::Chunked;
    BodyDecoder d(r, 40000000, 42000000);
    uint64_t n = 0;
    assert(feed(d, ending, n, true) == BodyState::Error);
  }
  {
    HttpRequest r{};
    r.framing = Framing::Chunked;
    BodyDecoder d(r, 1, 4);
    uint64_t n = 0;
    assert(feed(d, "0\r\n\r\n", n, false) == BodyState::Error);
  }
  {
    HttpRequest r{};
    r.framing = Framing::Chunked;
    BodyDecoder d(r, 1, 20);
    uint64_t n = 0;
    assert(feed(d, "2\r\nxx\r\n0\r\n\r\n", n, false) == BodyState::Error);
  }
  puts("PASS HTTP data-chunk budget + terminal zero + fragmentation + strict framing/quota controls");
}
