#include <cassert>

#include "AllocationBudget.h"
using namespace ink::http;
int main() {
  AllocationBudget b;
  int x = 0, y = 0;
  assert(b.permits(81920));
  assert(!b.permits(81921));
  assert(b.add(&x, 81900));
  assert(!b.permits(21));
  assert(b.permits(20));
  b.remove(&y);
  assert(!b.permits(21));
  b.remove(&x);
  assert(b.used() == 0);
  assert(!b.add(&x, 81921));
  assert(!b.add(nullptr, 1));
  int allocations[128]{};
  for (auto& allocation : allocations) assert(b.add(&allocation, 1));
  assert(b.used() == 128 && !b.permits(1));
  b.remove(nullptr);
  b.remove(&y);
  assert(b.used() == 128);
  b.remove(&allocations[64]);
  b.remove(&allocations[64]);
  assert(b.used() == 127 && b.permits(1));
  assert(b.add(&y, 1));
  for (auto& allocation : allocations) b.remove(&allocation);
  b.remove(&y);
  assert(b.used() == 0);
}
