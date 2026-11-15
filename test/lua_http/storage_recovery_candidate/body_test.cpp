#include "BodyStage.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace recoverable;
struct Disk : StageFiles {
  std::array<std::vector<uint8_t>,2> slots;
  unsigned calls=0, fail=0, touched=9, closes=0; bool failClose=false;
  bool step() { return ++calls != fail; }
  SlotState size(unsigned s,uint64_t& n) override { n=slots[s].size(); return n?SlotState::valid:SlotState::absent; }
  bool read(unsigned s,uint64_t o,uint8_t* p,size_t n) override {
    if(o+n>slots[s].size())return false;
    for(size_t i=0;i<n;++i)p[i]=slots[s][o+i];
    return true;
  }
  bool reset(unsigned s) override { touched=s; slots[s].clear(); return step(); }
  bool write(unsigned s,uint64_t o,const uint8_t* p,size_t n) override {
    touched=s; if(slots[s].size()<o+n)slots[s].resize(o+n);
    for(size_t i=0;i<n;++i)slots[s][o+i]=p[i];
    return step(); // deliberately mutates before reporting failure
  }
  bool sync(unsigned) override {return step();}
  bool close(unsigned) override {++closes; const bool ok=step(); return ok && !failClose;}
};
struct Input : BodyInput {
  unsigned reads=0, cancelAfter=999, failRead=0;
  bool read(uint32_t o,uint8_t* p,size_t n) override {
    ++reads; for(size_t i=0;i<n;++i)p[i]=uint8_t(o+i); return reads!=failRead;
  }
  bool cancelled() override { return reads>=cancelAfter; }
};
int main(){
 Disk d; Input in; WritePlan p{AdmissionState::ready,1,2};
 const auto old=std::vector<uint8_t>{4,5,6};d.slots[0]=old;
 auto r=stageBody(d,in,p,2500);
 assert(r==StageState::staged); assert(d.slots[0]==old && d.touched==1);
 assert(d.slots[1].size()==2548);
 for(unsigned i=0;i<24;++i)assert(d.slots[1][i]==0);
 for(unsigned i=0;i<2500;++i)assert(d.slots[1][i+24]==uint8_t(i));
 for(unsigned i=2524;i<2548;++i)assert(d.slots[1][i]==0);
 assert(RecoverableDownloadStore(d).validate(1).state==SlotState::invalid);
 std::puts("PASS body staged bounded and unsealed without touching old slot");
 const unsigned operations=d.calls;
 for(unsigned fail=1;fail<=operations;++fail){
   Disk broken; Input source; broken.slots[0]=old; broken.fail=fail;
   assert(stageBody(broken,source,p,2500)==StageState::io_error);
   assert(broken.slots[0]==old);
   assert(RecoverableDownloadStore(broken).validate(1).state!=SlotState::valid);
 }
 std::puts("PASS every mutating operation failure preserves selected slot and leaves candidate invalid");
 for(unsigned when=0;when<=3;++when){
   Disk cancelled; Input source; source.cancelAfter=when; cancelled.slots[0]=old;
   assert(stageBody(cancelled,source,p,2500)==StageState::cancelled);
   assert(cancelled.slots[0]==old);
   assert(RecoverableDownloadStore(cancelled).validate(1).state!=SlotState::valid);
 }
 std::puts("PASS cancellation before and between body chunks never publishes");
 for(auto bad: {WritePlan{AdmissionState::invalid,1,2},WritePlan{AdmissionState::ready,2,2},WritePlan{AdmissionState::ready,1,0}}){
   Disk denied; Input source; assert(stageBody(denied,source,bad,1)==StageState::rejected);assert(denied.calls==0);
 }
 Disk denied; Input source;
 assert(stageBody(denied,source,p,MaxPayload+1)==StageState::rejected && denied.calls==0);
 std::puts("PASS invalid plans and oversized body rejected before mutation");
 Disk empty; Input noBody;
 assert(stageBody(empty,noBody,p,0)==StageState::staged && empty.slots[1].size()==48);
 assert(noBody.reads==0 && RecoverableDownloadStore(empty).validate(1).state==SlotState::invalid);
 std::puts("PASS empty body remains unsealed");
 for(unsigned read=1;read<=3;++read){
   for(bool closeFailure: {false,true}){
     Disk broken;Input input;broken.slots[0]=old;broken.failClose=closeFailure;input.failRead=read;
     assert(stageBody(broken,input,p,2500)==StageState::io_error);
     assert(input.reads==read && broken.closes==1 && broken.slots[0]==old);
     assert(RecoverableDownloadStore(broken).validate(1).state!=SlotState::valid);
   }
 }
 std::puts("PASS source read failures with simultaneous close failure never publish");
 for(unsigned when=0;when<=3;++when){
   Disk broken;Input input;broken.slots[0]=old;broken.failClose=true;input.cancelAfter=when;
   assert(stageBody(broken,input,p,2500)==StageState::cancelled);
   assert(broken.closes==(when?1U:0U) && broken.slots[0]==old);
   assert(RecoverableDownloadStore(broken).validate(1).state!=SlotState::valid);
 }
 std::puts("PASS cancellation with close failure leaves unsealed candidate and old slot intact");
}
