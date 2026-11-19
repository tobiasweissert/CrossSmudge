#pragma once
#include "HttpCore.h"
namespace ink::http {
struct BodyStream {
  void* context;
  int (*read)(void*, char*, size_t);
  bool (*ok)(void*);
};
// One gzip member only; bounds, header CRC, payload CRC/ISIZE and trailing bytes
// are checked. The workspace is browser-owned PSRAM, never a framebuffer loan.
bool gzip(BodyStream&, Result&);
}  // namespace ink::http
