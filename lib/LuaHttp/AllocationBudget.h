#pragma once
#include <cstddef>
namespace ink::http {
// Used only by the single TLS worker. Tracks actual allocation sizes, including allocator rounding.
class AllocationBudget {
 public:
  static constexpr size_t Limit=80*1024;
  bool permits(size_t n) const {return n<=Limit-total_ && count_<128;}
  bool add(void* p,size_t n) {
    if(!p || !permits(n))return false;
    for(auto& slot:slots_)if(!slot.pointer){slot={p,n};total_+=n;++count_;return true;}
    return false;
  }
  void remove(void* p){for(auto& slot:slots_)if(slot.pointer==p && p){total_-=slot.bytes;--count_;slot={};return;}}
  size_t used()const{return total_;}
 private:
  struct Slot {void* pointer=nullptr;size_t bytes=0;};
  Slot slots_[128];size_t total_=0,count_=0;
};
}
