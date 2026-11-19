#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>

#include "BrowserImage.h"
int main() {
  const char* fixture = getenv("ASSISTED_PNG_FIXTURE");
  assert(fixture);
  std::ifstream in(fixture, std::ios::binary);
  assert(in);
  Storage.put("/palette.png", std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), {}));
  ink::browser::ImageSession session;
  assert(!session.start("/palette.png", "/palette.pxc", 128, 64));
  auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  int state = 0;
  while (!(state = session.poll()) && std::chrono::steady_clock::now() < until) std::this_thread::yield();
  assert(state == 1);
  GfxRenderer renderer;
  std::atomic<bool> cancel{false};
  assert(session.draw("/palette.pxc", renderer, 0, 0, 128, 64, cancel));
  auto* pixels = renderer.getWriteTarget();
  unsigned ink = 0;
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 128; ++x) ink += (pixels[y * 100 + x / 8] & (0x80 >> (x % 8))) == 0;
  assert(ink == 4096);
  assert(!Storage.openHandles("/palette.png"));
  assert(!Storage.openHandles("/palette.pxc"));
  puts("PASS actual async PNGdec -> 2-bit PXC -> blit: 128x64, exactly 4096 black pixels; closed resources");
}
