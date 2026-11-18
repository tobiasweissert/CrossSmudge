#include <cassert>
#include <cstring>
#include <map>
#include <string>

#include "HalBrowserBackend.h"
std::map<std::string, std::string> nvs;
std::string sdConfig;
bool readable = false, closed = false, wifi = true;
int failWrite = 0, failRead = 0;
int main() {
  auto a = HalBrowserBackend::access();
  assert(!a.read && !a.import);
  assert(nvs.empty() && !closed);
  unsigned char bytes[16];
  wifi = false;
  assert(!a.random(nullptr, bytes, 16));
  wifi = true;
  assert(a.random(nullptr, bytes, 16));
  assert(!memcmp(bytes, "\0\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17", 16));
  assert(nvs.empty() && !closed);  // no SD/NVS setup even when private file is absent
}
