#include "src/network/CrosscubOtaPolicy.h"
#include <cassert>
#include <cstring>
#include <initializer_list>

int main() {
  using namespace crosscub_ota;
  const char* tag = "crosscub-v1.6.5.4";
  const char* good = "https://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.4/crosscub-sticky-v1.6.5.4.bin";
  const char* digest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  assert(version("crosscub-v99999999999") == nullptr);
  assert(version("crosscub-v1.2.3.4444444444") == nullptr);
  assert(acceptsDownload(tag, good, digest, 65536));
  for (const char* bad : {"v99.0", "crosscub-v", "crosscub-v1/evil", "crosscub-v1?x", "crosscub-v1%2f"})
    assert(!acceptsDownload(bad, good, digest, 65536));
  for (const char* bad : {
      "https://github.com/Mumfee/CrossSmudge/releases/download/crosscub-v1.6.5.4/crosscub-sticky-v1.6.5.4.bin",
      "https://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.4/firmware-sticky-v1.6.5.4.bin",
      "https://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.4/crosscub-sticky-v1.6.5.3.bin",
      "https://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.4/crosscub-sticky-v1.6.5.4.bin?x",
      "http://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.4/crosscub-sticky-v1.6.5.4.bin"})
    assert(!acceptsDownload(tag, bad, digest, 65536));
  assert(!acceptsDownload(tag, good, "", 65536));
  assert(!acceptsDownload(tag, good, digest, 65535));
  uint8_t image[112] = {};
  image[0] = 0xe9;
  image[32] = 0x32; image[33] = 0x54; image[34] = 0xcd; image[35] = 0xab;
  for (const char* name : {"CrossSmudge", "CrossSmudgeStickyTest", "CrosscubFake"}) {
    std::memset(image + 80, 0, 32); std::strcpy(reinterpret_cast<char*>(image + 80), name);
    assert(!acceptsImageDescriptor(image, sizeof(image)));
  }
  std::memset(image + 80, 0, 32); std::strcpy(reinterpret_cast<char*>(image + 80), "Crosscub");
  assert(acceptsImageDescriptor(image, sizeof(image)));
  assert(!acceptsImageDescriptor(image, 111));
  image[32] = 0;
  assert(!acceptsImageDescriptor(image, sizeof(image)));
}
