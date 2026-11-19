#pragma once
#include <cstddef>
inline void esp_fill_random(void* p, size_t n) {
  static unsigned char seed = 0;
  auto b = static_cast<unsigned char*>(p);
  while (n--) *b++ = ++seed;
}
