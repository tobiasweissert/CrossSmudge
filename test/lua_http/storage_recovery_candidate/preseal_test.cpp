#include "PreSealCheck.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace recoverable;
struct Disk : Files {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(2548);
  unsigned reads=0, failRead=0; bool sizeFails=false;
  Disk() { for(unsigned i=0;i<2500;++i)bytes[i+24]=uint8_t(i); }
  SlotState size(unsigned slot,uint64_t& n) override {
    assert(slot==1); n=bytes.size(); return sizeFails?SlotState::uncertain:SlotState::valid;
  }
  bool read(unsigned slot,uint64_t offset,uint8_t* out,size_t n) override {
    assert(slot==1 && n<=1024); if(++reads==failRead)return false;
    assert(offset+n<=bytes.size());
    for(size_t i=0;i<n;++i)out[i]=bytes[offset+i];
    return true;
  }
};
int main(){
 Disk good; const auto original=good.bytes;
 const uint32_t checksum=0x817060f1U; // independently calculated sequence fixture
 assert(checkStaged(good,1,2500,checksum)==PreSealState::verified);
 assert(good.bytes==original);
 std::puts("PASS exact staged body and zero metadata verified read-only");
 const unsigned reads=good.reads;
 for(unsigned i=1;i<=reads;++i){Disk d;d.failRead=i;assert(checkStaged(d,1,2500,checksum)==PreSealState::uncertain);assert(d.bytes==original);}
 Disk uncertain;uncertain.sizeFails=true;
 assert(checkStaged(uncertain,1,2500,checksum)==PreSealState::uncertain);
 std::puts("PASS every read and size uncertainty fail closed");
 for(unsigned i=0;i<original.size();++i){Disk d;d.bytes[i]^=1;assert(checkStaged(d,1,2500,checksum)==PreSealState::invalid);}
 std::puts("PASS every staged body header and footer byte corruption rejected");
 Disk shortFile;shortFile.bytes.pop_back();assert(checkStaged(shortFile,1,2500,checksum)==PreSealState::invalid);
 Disk longFile;longFile.bytes.push_back(0);assert(checkStaged(longFile,1,2500,checksum)==PreSealState::invalid);
 Disk denied;assert(checkStaged(denied,2,2500,checksum)==PreSealState::invalid && denied.reads==0);
 assert(checkStaged(denied,1,MaxPayload+1,checksum)==PreSealState::invalid && denied.reads==0);
 assert(checkStaged(denied,1,2500,checksum^1)==PreSealState::invalid);
 std::puts("PASS extent slot payload limit and expected checksum enforced");
 Disk empty;empty.bytes.assign(48,0);
 assert(checkStaged(empty,1,0,2166136261U)==PreSealState::verified);
 std::puts("PASS empty staged body verified without publication");
}
