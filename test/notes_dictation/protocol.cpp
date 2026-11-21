#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/activities/notes/DictationProtocol.h"
int main() {
  using namespace dictation;
  uint8_t h[44]{};
  assert(!wav(h, 0));
  assert(!wav(h, 3));
  assert(!wav(h, maxPcm + 2));
  assert(wav(h, maxPcm));
  assert(!memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVEfmt ", 8) && !memcmp(h + 36, "data", 4));
  assert(h[24] == 0x80 && h[25] == 0x3e && h[22] == 1 && h[34] == 16);
  assert(statusError(401) && statusError(403) && statusError(429) && statusError(302) && !statusError(200));
  assert(validKey("fixture-neutral", 15) && !validKey("bad\rkey", 7) && !validKey("", 0));
  puts("PASS WAV bounds and format; auth, redirect and quota errors; key header validation");
}
