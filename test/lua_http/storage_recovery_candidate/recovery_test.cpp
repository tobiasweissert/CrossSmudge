#include "RecoverableDownloadStore.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace recoverable;
// Synthetic adapter, NOT SdFat or a block-device fixture.
struct MemoryFiles : Files {
  std::array<std::vector<uint8_t>,2> data;
  std::array<bool,2> present{false,false};
  bool fail_size = false;
  int fail_read = -1, reads = 0;
  size_t max_read = 0;
  SlotState size(unsigned s, uint64_t& n) override {
    if (fail_size) return SlotState::uncertain;
    if (!present[s]) return SlotState::absent;
    n=data[s].size(); return SlotState::valid;
  }
  bool read(unsigned s,uint64_t off,uint8_t* out,size_t n) override {
    max_read=std::max(max_read,n);
    if (reads++==fail_read || off>data[s].size() || n>data[s].size()-off) return false;
    std::memcpy(out,data[s].data()+off,n);return true;
  }
};
static void put(std::vector<uint8_t>& b,size_t at,uint64_t v,unsigned n) {
  for (unsigned i=0;i<n;++i) b[at+i]=uint8_t(v>>(8*i));
}
// Independent authored format fixture. 24-byte header, payload, 24-byte seal.
static std::vector<uint8_t> record(uint64_t gen,const std::vector<uint8_t>& body) {
  std::vector<uint8_t> b(48+body.size(),0);
  const uint8_t magic[]={0x52,0x44,0x53,0x31};
  std::copy(magic,magic+4,b.begin());put(b,4,1,4);put(b,8,gen,8);put(b,16,body.size(),4);
  uint32_t hash=2166136261U;for(auto c:body) hash=(hash^c)*16777619U;
  put(b,20,hash,4);std::copy(body.begin(),body.end(),b.begin()+24);
  size_t footer=24+body.size();const uint8_t seal[]={0x53,0x45,0x41,0x4c};
  std::copy(seal,seal+4,b.begin()+footer);put(b,footer+4,1,4);
  put(b,footer+8,gen,8);put(b,footer+16,body.size(),4);put(b,footer+20,hash,4);
  return b;
}
int main() {
  MemoryFiles f;f.present[0]=true;f.data[0]=record(1,{'n','e','w'});
  RecoverableDownloadStore store(f);auto r=store.validate(0);
  assert(r.state==SlotState::valid);assert(r.generation==1);assert(r.length==3);
  std::puts("PASS valid sealed record");
  f.present[1]=true;f.data[1]=record(2,{'n','e','x','t'});
  auto recovered=store.recover();assert(recovered.state==RecoveryState::selected);
  assert(recovered.slot==1);assert(recovered.record.generation==2);
  std::puts("PASS newest valid generation recovered");
  auto pristine=f.data;
  // Every byte in this short complete record participates in validation.
  for(size_t i=0;i<pristine[1].size();++i) {
    f.data=pristine;f.data[1][i]^=0x80;
    assert(store.validate(1).state==SlotState::invalid);
    auto old=store.recover();assert(old.state==RecoveryState::selected && old.slot==0);
  }
  std::puts("PASS every-byte corruption preserves prior generation");
  f.data=pristine;f.data[1].push_back(0);assert(store.validate(1).state==SlotState::invalid);
  for(size_t n=0;n<pristine[1].size();++n) {
    f.data[1]=pristine[1];f.data[1].resize(n);assert(store.validate(1).state==SlotState::invalid);
  }
  std::puts("PASS truncated and trailing records rejected");
  f.data=pristine;f.data[1]=record(0,{});assert(store.validate(1).state==SlotState::invalid);
  f.present[1]=false;assert(store.validate(1).state==SlotState::absent);
  f.fail_size=true;assert(store.recover().state==RecoveryState::uncertain);f.fail_size=false;
  f.present[1]=true;f.data=pristine;
  for(int read=0;read<6;++read) {
    f.reads=0;f.fail_read=read;assert(store.recover().state==RecoveryState::uncertain);
  }
  f.fail_read=-1;std::puts("PASS absence distinguished from size and read uncertainty");
  f.data[1]=record(1,{'o','t','h','e','r'});assert(store.recover().state==RecoveryState::conflict);
  f.data[0]=record(5,{0xae,0x0a,0x4a,0x5d,0xec,0xad,0xf6,0xb1});
  f.data[1]=record(5,{0x94,0xfa,0xad,0x35,0x47,0x50,0x64,0xc8});
  assert(store.validate(0).checksum==store.validate(1).checksum);
  assert(store.recover().state==RecoveryState::conflict);
  std::puts("PASS equal-generation conflicts include actual checksum collision");
  f.data[1]=f.data[0];assert(store.recover().state==RecoveryState::selected);
  // Six validation reads followed by the equality comparison read.
  f.reads=0;f.fail_read=6;assert(store.recover().state==RecoveryState::uncertain);f.fail_read=-1;
  std::puts("PASS identical equal generations and comparison I/O failure");
  std::vector<uint8_t> large(MaxPayload,0x5a);f.data[1]=record(UINT64_MAX,large);
  auto maximum=store.recover();assert(maximum.state==RecoveryState::selected);
  assert(maximum.record.generation==UINT64_MAX && maximum.record.length==MaxPayload);
  assert(f.max_read<=1024);large.push_back(0);f.data[1]=record(6,large);
  assert(store.validate(1).state==SlotState::invalid);
  assert(store.validate(2).state==SlotState::invalid);
  std::puts("PASS 2 MiB limit, bounded reads, numeric-slot admission, max generation read");
  f.present={false,false};assert(store.recover().state==RecoveryState::empty);
  std::puts("PASS empty storage");
}
