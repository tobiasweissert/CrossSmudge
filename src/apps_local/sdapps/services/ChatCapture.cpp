#include "ChatCapture.h"

#include <algorithm>
#include <cstring>
namespace stickychat {
namespace {
void put16(uint8_t* p, uint16_t n) {
  p[0] = n;
  p[1] = n >> 8;
}
void put32(uint8_t* p, uint32_t n) {
  for (int i = 0; i < 4; ++i) p[i] = n >> (i * 8);
}
}  // namespace
bool Capture::start(uint8_t* storage, size_t capacity, uint32_t now) {
  cancel();
  if (!storage || capacity < kWavMax) return false;
  bytes = storage;
  used = 0;
  started = now;
  if (!mic.begin()) {
    mic.end();
    bytes = nullptr;
    return false;
  }
  live = true;
  return true;
}
void Capture::tick(uint32_t now) {
  if (!live) return;
  if (static_cast<uint32_t>(now - started) >= 30000) {
    stop();
    return;
  }
  const size_t wanted = std::min<size_t>(256, kPcmMax - used);
  const size_t got = mic.read(bytes + 44 + used, wanted);
  if (!got || got > wanted || got % 2) {
    cancel();
    return;
  }
  used += got;
  if (used == kPcmMax) stop();
}
void Capture::stop() {
  if (!live) return;
  mic.end();
  live = false;
  valid = used > 0;
  if (!valid) return;
  memset(bytes, 0, 44);
  memcpy(bytes, "RIFF", 4);
  put32(bytes + 4, used + 36);
  memcpy(bytes + 8, "WAVEfmt ", 8);
  put32(bytes + 16, 16);
  put16(bytes + 20, 1);
  put16(bytes + 22, 1);
  put32(bytes + 24, 16000);
  put32(bytes + 28, 32000);
  put16(bytes + 32, 2);
  put16(bytes + 34, 16);
  memcpy(bytes + 36, "data", 4);
  put32(bytes + 40, used);
}
void Capture::cancel() {
  if (live) mic.end();
  live = false;
  valid = false;
  used = 0;
  bytes = nullptr;
}
}  // namespace stickychat
