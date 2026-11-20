#include "AllocationObserver.h"

#include <cassert>
#include <cstdlib>
#include <new>
bool forbidAllocation = false;
extern "C" {
void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void __real_free(void*);
void* __wrap_malloc(size_t size) {
  assert(!forbidAllocation);
  return __real_malloc(size);
}
void* __wrap_calloc(size_t a, size_t b) {
  assert(!forbidAllocation);
  return __real_calloc(a, b);
}
void* __wrap_realloc(void* p, size_t size) {
  assert(!forbidAllocation);
  return __real_realloc(p, size);
}
void __wrap_free(void* p) {
  assert(!forbidAllocation);
  __real_free(p);
}
}
void* operator new(size_t n) {
  assert(!forbidAllocation);
  if (auto* p = std::malloc(n)) return p;
  throw std::bad_alloc();
}
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  assert(!forbidAllocation);
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, size_t) noexcept { ::operator delete(p); }
