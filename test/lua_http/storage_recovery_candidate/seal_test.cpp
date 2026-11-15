#include "SealStage.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace recoverable;
struct Memory : SealFiles {
  std::vector<uint8_t> slots[2];
  int writes=0, failWrite=0; size_t prefix=0;
  bool failSync=false, failClose=false, failRead=false, failOpen=false;
  bool opened=false, cancel=false, cancelOnWrite=false;
  SlotState size(unsigned s,uint64_t& n) override { n=slots[s].size(); return n?SlotState::valid:SlotState::absent; }
  bool read(unsigned s,uint64_t o,uint8_t* p,size_t n) override {
    if(failRead && writes) return false;
    if(o+n>slots[s].size()) return false;
    std::memcpy(p,slots[s].data()+o,n); return true;
  }
  bool reset(unsigned s) override { slots[s].clear(); return true; }
  bool openSeal(unsigned) override { opened=true; return !failOpen; }
  bool write(unsigned s,uint64_t o,const uint8_t* p,size_t n) override {
    ++writes; bool bad=writes==failWrite; size_t k=bad?prefix:n;
    assert(k<=n); if(o+k>slots[s].size()) slots[s].resize(o+k);
    std::memcpy(slots[s].data()+o,p,k); if(cancelOnWrite) cancel=true;
    return !bad;
  }
  bool sync(unsigned) override { return !failSync; }
  bool close(unsigned) override { opened=false; return !failClose; }
};
struct Input : BodyInput {
  Memory& m; explicit Input(Memory& x):m(x){}
  bool read(uint32_t o,uint8_t* p,size_t n) override {for(size_t i=0;i<n;++i)p[i]=uint8_t(o+i);return true;}
  bool cancelled() override {return m.cancel;}
};
StageReceipt staged(Memory& m) {
 Input i(m); auto p=admitWrite(RecoverableDownloadStore(m).recover(),37,false); StageReceipt r;
 assert(stageBody(m,i,p,37,&r)==StageState::staged); m.writes=0; return r;
}
Memory prior() {
 Memory m; Input i(m); auto r=staged(m);
 assert(sealStaged(m,i,r)==CommitState::committed);
 assert(RecoverableDownloadStore(m).recover().record.generation==1);
 m.writes=0; return m;
}
int main(){
 {auto m=prior(); auto old=m.slots[0]; auto r=staged(m); Input i(m);
  assert(sealStaged(m,i,r)==CommitState::committed);
  auto got=RecoverableDownloadStore(m).recover(); assert(got.slot==1 && got.record.generation==2);
  assert(m.slots[0]==old); puts("PASS seal publishes verified newer generation without changing old slot");}
 {for(int w=1;w<=2;++w)for(size_t n=0;n<=24;++n){auto m=prior();auto r=staged(m);auto old=m.slots[0];Input i(m);m.failWrite=w;m.prefix=n;
  assert(sealStaged(m,i,r)==CommitState::commit_unknown);assert(m.slots[0]==old);
  m.failWrite=0; auto got=RecoverableDownloadStore(m).recover();assert(got.state==RecoveryState::selected);assert(got.record.generation==1||got.record.generation==2);
 } puts("PASS every partial and fully mutated failed seal write is indeterminate and recoverable");}
 {for(int f=0;f<3;++f){auto m=prior();auto r=staged(m);auto old=m.slots[0];Input i(m);
  m.failSync=f==0;m.failClose=f==1;m.failRead=f==2;
  assert(sealStaged(m,i,r)==CommitState::commit_unknown);assert(m.slots[0]==old);
  m.failSync=m.failClose=m.failRead=false;auto got=RecoverableDownloadStore(m).recover();assert(got.record.generation==2);
 }puts("PASS sync close and readback failures preserve ambiguous publication until recovery");}
 {auto m=prior();auto r=staged(m);Input i(m);auto before=m.slots[1];m.cancel=true;
  assert(sealStaged(m,i,r)==CommitState::not_committed);assert(m.slots[1]==before);assert(m.writes==0);
  m.cancel=false;m.cancelOnWrite=true;assert(sealStaged(m,i,r)==CommitState::committed);
  puts("PASS cancellation admitted before seal and cannot report no-change after seal");}
 {for(int f=0;f<5;++f){auto m=prior();auto r=staged(m);Input i(m);
  if(f==0) r.valid=false;
  if(f==1) r.generation=1;
  if(f==2) r.slot=0;
  if(f==3) m.slots[1][24]^=1;
  if(f==4) m.failOpen=true;
  auto a=m.slots[0],b=m.slots[1];assert(sealStaged(m,i,r)==CommitState::not_committed);
  assert(m.slots[0]==a&&m.slots[1]==b&&m.writes==0);
 }puts("PASS invalid stale corrupt and unopened candidates reject before seal mutation");}
}
