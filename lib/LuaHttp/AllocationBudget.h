#pragma once
#include <algorithm>
#include <cstddef>
#include <iterator>
namespace ink::http {
// Used only by the single TLS worker. Tracks actual allocation sizes, including allocator rounding.
class AllocationBudget {
 public:
  static constexpr size_t Limit = 80 * 1024;
  bool permits(size_t n) const { return n <= Limit - total_ && count_ < 128; }
  bool add(void* p, size_t n) {
    if (!p || !permits(n)) return false;
    auto* slot = std::find_if(std::begin(slots_), std::end(slots_), [](const Slot& entry) { return !entry.pointer; });
    if (slot == std::end(slots_)) return false;
    *slot = {p, n};
    total_ += n;
    ++count_;
    return true;
  }
  void remove(const void* p) {
    if (!p) return;
    auto* slot =
        std::find_if(std::begin(slots_), std::end(slots_), [p](const Slot& entry) { return entry.pointer == p; });
    if (slot == std::end(slots_)) return;
    total_ -= slot->bytes;
    --count_;
    *slot = {};
  }
  size_t used() const { return total_; }

 private:
  struct Slot {
    void* pointer = nullptr;
    size_t bytes = 0;
  };
  Slot slots_[128];
  size_t total_ = 0, count_ = 0;
};
}  // namespace ink::http
