#include "AllocationBudget.h"
#include <cassert>
using namespace ink::http;
int main(){AllocationBudget b;int x,y;
 assert(b.permits(81920));assert(!b.permits(81921));
 assert(b.add(&x,81900));assert(!b.permits(21));assert(b.permits(20));
 b.remove(&y);assert(!b.permits(21));b.remove(&x);assert(b.used()==0);
 assert(!b.add(&x,81921));
}
