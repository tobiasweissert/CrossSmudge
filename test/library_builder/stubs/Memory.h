#pragma once

#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <map>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "HalStorage.h"
namespace fake {
inline std::map<void*, size_t> liveHeapBuffers;
inline unsigned heapAllocationAttempts = 0;
inline void* libraryMalloc(size_t n) {
  ++heapAllocationAttempts;
  if (fail(failAlloc)) return nullptr;
  void* ptr = std::malloc(n);
  if (ptr) liveHeapBuffers[ptr] = n;
  return ptr;
}
inline void libraryFree(void* ptr) {
  liveHeapBuffers.erase(ptr);
  std::free(ptr);
}
}  // namespace fake
namespace std {
inline void* libraryMalloc(size_t n) { return fake::libraryMalloc(n); }
inline void libraryFree(void* p) { fake::libraryFree(p); }
}  // namespace std
#define makeUniqueNoThrow productionMakeUniqueNoThrow
#define malloc libraryMalloc
#define free libraryFree
#include "../../../lib/Memory/Memory.h"
#undef free
#undef malloc
#undef makeUniqueNoThrow

template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
std::unique_ptr<T> makeUniqueNoThrow(Args&&... args) {
  if (fake::fail(fake::failAlloc)) return nullptr;
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
  requires std::is_unbounded_array_v<T>
std::unique_ptr<T> makeUniqueNoThrow(size_t count) {
  if (fake::fail(fake::failAlloc)) return nullptr;
  return std::make_unique<T>(count);
}
