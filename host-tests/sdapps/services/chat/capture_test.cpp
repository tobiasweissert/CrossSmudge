#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ChatCapture.h"
using namespace stickychat;
// Fake hardware boundary only; production Capture builds the WAV bytes.
struct FakeMic : Microphone {
  bool on = false, fail = false, odd = false;
  int ends = 0;
  bool begin() override {
    on = true;
    return !fail;
  }
  size_t read(uint8_t* out, size_t max) override {
    assert(on);
    if (max > 320) return 0; /* Fake 10 ms / 16 kHz timeout boundary. */
    memset(out, 0x42, max);
    return odd ? 3 : max;
  }
  void end() override {
    on = false;
    ++ends;
  }
};
int main() {
  std::vector<uint8_t> bytes(kWavMax);
  FakeMic mic;
  Capture c(mic);
  assert(!mic.on && !c.size());
  assert(c.start(bytes.data(), bytes.size(), 100));
  c.tick(101);
  assert(c.recording());
  c.stop();
  assert(!mic.on && !c.recording() && c.size() == 300);
  assert(!memcmp(bytes.data(), "RIFF", 4) && !memcmp(bytes.data() + 8, "WAVEfmt ", 8));
  assert(bytes[22] == 1 && bytes[24] == 0x80 && bytes[25] == 0x3e && bytes[34] == 16);
  c.cancel();
  assert(c.size() == 0 && !mic.on);
  assert(!c.start(bytes.data(), 100, 0));
  mic.fail = true;
  assert(!c.start(bytes.data(), bytes.size(), 0));
  assert(!mic.on);
  mic.fail = false;
  assert(c.start(bytes.data(), bytes.size(), 0));
  c.tick(30000);
  assert(!c.recording() && !mic.on && !c.size());
  assert(c.start(bytes.data(), bytes.size(), 0));
  for (int i = 0; i < 4000; ++i) c.tick(i);
  assert(!c.recording() && c.size() == kWavMax && !mic.on);
  c.cancel();
  mic.odd = true;
  assert(c.start(bytes.data(), bytes.size(), 0));
  c.tick(1);
  assert(!c.recording() && !c.size() && !mic.on);
  std::puts("capture cleanup/deadline/WAV bounds (fake mic): PASS");
}
