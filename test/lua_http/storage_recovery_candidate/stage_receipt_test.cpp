#include "BodyStage.h"
#include "PreSealCheck.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace recoverable;
struct Memory : StageFiles {
  std::array<std::vector<uint8_t>,2> slots;
  unsigned calls=0, fail=0;
  bool step(){return ++calls!=fail;}
  SlotState size(unsigned s,uint64_t& n) override {n=slots[s].size();return n?SlotState::valid:SlotState::absent;}
  bool read(unsigned s,uint64_t o,uint8_t* p,size_t n) override {
    if(o+n>slots[s].size())return false;
    for(size_t i=0;i<n;++i)p[i]=slots[s][o+i];
    return true;
  }
  bool reset(unsigned s) override {slots[s].clear();return step();}
  bool write(unsigned s,uint64_t o,const uint8_t* p,size_t n) override {
    if(slots[s].size()<o+n)slots[s].resize(o+n);
    for(size_t i=0;i<n;++i)slots[s][o+i]=p[i];
    return step();
  }
  bool sync(unsigned) override {return step();}
  bool close(unsigned) override {return step();}
};
struct Source : BodyInput {
  unsigned reads=0, cancelAfter=999, failRead=0;
  bool read(uint32_t o,uint8_t* p,size_t n) override {
    ++reads;
    for(size_t i=0;i<n;++i)p[i]=uint8_t(o+i);
    return reads!=failRead;
  }
  bool cancelled() override {return reads>=cancelAfter;}
};
int main(){
  Memory disk; Source source; WritePlan plan{AdmissionState::ready,1,2};
  StageReceipt receipt;
  assert(stageBody(disk,source,plan,2500,&receipt)==StageState::staged);
  assert(receipt.valid && receipt.slot==1 && receipt.generation==2 && receipt.length==2500);
  uint32_t expected=2166136261U;
  for(unsigned i=0;i<2500;++i)expected=(expected^uint8_t(i))*16777619U;
  assert(receipt.checksum==expected);
  assert(checkStaged(disk,receipt.slot,receipt.length,receipt.checksum)==PreSealState::verified);
  assert(RecoverableDownloadStore(disk).validate(1).state==SlotState::invalid);
  std::puts("PASS accepted input checksum reaches read-only preseal verification without publication");
  const unsigned operations=disk.calls;
  for(unsigned fail=1;fail<=operations;++fail){
    Memory broken; Source input; broken.fail=fail;
    StageReceipt stale=receipt;
    assert(stageBody(broken,input,plan,2500,&stale)==StageState::io_error);
    assert(!stale.valid && !stale.generation && !stale.length && !stale.checksum);
  }
  for(unsigned when=0;when<=3;++when){
    Memory broken; Source input; input.cancelAfter=when; StageReceipt stale=receipt;
    assert(stageBody(broken,input,plan,2500,&stale)==StageState::cancelled);
    assert(!stale.valid && !stale.generation && !stale.length && !stale.checksum);
  }
  for(unsigned read=1;read<=3;++read){
    Memory broken;Source input;input.failRead=read;StageReceipt stale=receipt;
    assert(stageBody(broken,input,plan,2500,&stale)==StageState::io_error);
    assert(!stale.valid);
  }
  Memory denied;Source input;StageReceipt stale=receipt;
  assert(stageBody(denied,input,plan,MaxPayload+1,&stale)==StageState::rejected);
  assert(!stale.valid && denied.calls==0);
  std::puts("PASS failed cancelled and rejected staging clears stale receipts including close and sync errors");
  Memory empty;Source none;StageReceipt zero;
  assert(stageBody(empty,none,plan,0,&zero)==StageState::staged);
  assert(zero.valid && zero.length==0 && zero.checksum==2166136261U);
  assert(checkStaged(empty,zero.slot,zero.length,zero.checksum)==PreSealState::verified);
  std::puts("PASS empty-body receipt retains FNV offset basis and verifies unsealed extent");
}
