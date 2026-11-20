#include <HalStorage.h>

#include <cstddef>
#include <cstdlib>
#include <new>

namespace {
struct alignas(std::max_align_t) Header {
  bool checked;
};
void* allocate(size_t size, bool checked) noexcept {
  if (checked) {
    ++fake::checkedAllocationAttempts;
    if (fake::fail(fake::failAlloc)) return nullptr;
  }
  if (size > SIZE_MAX - sizeof(Header)) return nullptr;
  auto* p = static_cast<Header*>(std::malloc(sizeof(Header) + (size ? size : 1)));
  if (!p) return nullptr;
  p->checked = checked;
  if (checked) ++fake::liveCheckedAllocations;
  return p + 1;
}
void release(void* ptr) noexcept {
  if (!ptr) return;
  auto* p = static_cast<Header*>(ptr) - 1;
  if (p->checked) --fake::liveCheckedAllocations;
  std::free(p);
}
}  // namespace
void* operator new(size_t n) {
  if (void* p = allocate(n, false)) return p;
  std::abort();
}
void* operator new[](size_t n) { return ::operator new(n); }
void* operator new(size_t n, const std::nothrow_t&) noexcept { return allocate(n, true); }
void* operator new[](size_t n, const std::nothrow_t&) noexcept { return allocate(n, true); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, size_t) noexcept { release(p); }
void operator delete[](void* p, size_t) noexcept { release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }
