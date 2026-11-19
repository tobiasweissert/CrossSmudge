#pragma once
#include <cassert>
#include <cstring>
#include <string>
extern std::string sdConfig;
extern bool readable, closed;
class HalFile {
 public:
  explicit operator bool() const { return readable; }
  size_t size() { return sdConfig.size(); }
  int read(void* out, size_t n) {
    memcpy(out, sdConfig.data(), n);
    return n;
  }
  void close() { closed = true; }
};
struct FakeStorage {
  HalFile open(const char* path) {
    assert(!strcmp(path, "/.crosssmudge/private/browser-backend.json"));
    closed = false;
    return {};
  }
};
inline FakeStorage Storage;
