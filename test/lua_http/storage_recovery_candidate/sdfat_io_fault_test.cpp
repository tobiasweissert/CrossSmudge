// Host qualification only: real FAT, synthetic block read/sync failures.
#include "SdFatFixture.h"
#include <cassert>
#include <cstdio>
using namespace recoverable;
struct IoDisk : BlockDisk {
  unsigned reads=0, syncs=0, failRead=0, failSync=0;
  bool readSector(Sector_t s,uint8_t* p) override {
    if (++reads==failRead) return false;
    return BlockDisk::readSector(s,p);
  }
  bool syncDevice() override { return ++syncs!=failSync; }
};
struct Bytes : BodyInput {
  bool read(uint32_t o,uint8_t* p,size_t n) override {
    for(size_t i=0;i<n;++i) p[i]=uint8_t(o+i+17);
    return true;
  }
  bool cancelled() override { return false; }
};
int main() {
  IoDisk disk; uint8_t sector[512]; FatFormatter formatter;
  assert(formatter.format(&disk,sector));
  FatVolume initial; assert(initial.begin(&disk)); SdFatFiles files(initial);
  for(const char* name : {"slot0.dat","slot1.dat"}) {
    FatFile f; assert(f.open(&initial,name,O_CREAT|O_RDWR)); assert(f.close());
  }
  Bytes input; StageReceipt receipt;
  auto plan=admitWrite(RecoverableDownloadStore(files).recover(),2049,false);
  assert(stageBody(files,input,plan,2049,&receipt)==StageState::staged);
  assert(sealStaged(files,input,receipt)==CommitState::committed);
  auto baseline=disk.sectors;
  unsigned readCount=0;
  {
    FatVolume v; assert(v.begin(&disk)); SdFatFiles reader(v); disk.reads=0;
    assert(RecoverableDownloadStore(reader).recover().state==RecoveryState::selected);
    readCount=disk.reads;
  }
  assert(readCount>0);
  for(unsigned fail=1;fail<=readCount;++fail) {
    disk.sectors=baseline; disk.failRead=0;
    FatVolume v; assert(v.begin(&disk)); SdFatFiles reader(v);
    disk.reads=0; disk.failRead=fail;
    auto recovered=RecoverableDownloadStore(reader).recover();
    assert(disk.reads>=fail);
    assert(recovered.state==RecoveryState::uncertain);
    assert(admitWrite(recovered,2049,false).state!=AdmissionState::ready);
    disk.failRead=0;
    FatVolume clean; assert(clean.begin(&disk)); SdFatFiles cleanReader(clean);
    auto old=RecoverableDownloadStore(cleanReader).validate(0);
    assert(old.state==SlotState::valid && old.generation==1 && old.length==2049);
  }
  printf("PASS every actual FAT recovery block-read failure refuses write admission: %u injections\n",readCount);
  disk.sectors=baseline;
  FatVolume v; assert(v.begin(&disk)); SdFatFiles writer(v);
  plan=admitWrite(RecoverableDownloadStore(writer).recover(),2049,false);
  assert(stageBody(writer,input,plan,2049,&receipt)==StageState::staged);
  auto staged=disk.sectors;
  disk.syncs=0;
  assert(sealStaged(writer,input,receipt)==CommitState::committed);
  unsigned syncCount=disk.syncs;
  assert(syncCount>0);
  for(unsigned fail=1;fail<=syncCount;++fail) {
    disk.sectors=staged; disk.failSync=0;
    FatVolume trial; assert(trial.begin(&disk)); SdFatFiles target(trial);
    disk.syncs=0; disk.writes=0; disk.failSync=fail;
    auto outcome=sealStaged(target,input,receipt);
    if(outcome==CommitState::committed) std::fprintf(stderr,"false committed at sync %u of %u\n",fail,syncCount);
    assert(outcome==CommitState::not_committed || outcome==CommitState::commit_unknown);
    if(disk.writes>0) assert(outcome==CommitState::commit_unknown);
    assert(disk.syncs>=fail); disk.failSync=0;
    FatVolume clean; assert(clean.begin(&disk)); SdFatFiles reader(clean);
    auto old=RecoverableDownloadStore(reader).validate(0);
    assert(old.state==SlotState::valid && old.generation==1 && old.length==2049);
    auto result=RecoverableDownloadStore(reader).recover();
    assert(result.state==RecoveryState::selected);
    assert(result.record.generation==1 || result.record.generation==2);
  }
  printf("PASS every actual FAT seal device-sync rejection refuses success; post-write rejection is unknown; old slot intact: %u injections\n",syncCount);
  // Count from a fresh volume, then inject each operation at that same boundary.
  for(unsigned phase=0;phase<3;++phase) {
    disk.sectors=phase==2?staged:baseline;
    disk.failRead=0; disk.failSync=0;
    FatVolume countVolume; assert(countVolume.begin(&disk)); SdFatFiles countFiles(countVolume);
    auto next=admitWrite(RecoverableDownloadStore(countFiles).recover(),2049,false);
    disk.reads=0; disk.syncs=0;
    if(phase==2) assert(sealStaged(countFiles,input,receipt)==CommitState::committed);
    else { StageReceipt sample; assert(stageBody(countFiles,input,next,2049,&sample)==StageState::staged); }
    unsigned count=phase==0?disk.syncs:disk.reads;
    assert(count>0);
    for(unsigned fail=1;fail<=count;++fail) {
      disk.sectors=phase==2?staged:baseline;
      disk.failRead=0; disk.failSync=0;
      FatVolume trial; assert(trial.begin(&disk)); SdFatFiles target(trial);
      auto trialPlan=admitWrite(RecoverableDownloadStore(target).recover(),2049,false);
      disk.reads=0; disk.syncs=0; disk.writes=0;
      if(phase==0) disk.failSync=fail; else disk.failRead=fail;
      if(phase==2) {
        auto outcome=sealStaged(target,input,receipt);
        assert(outcome!=CommitState::committed);
        if(disk.writes>0) assert(outcome==CommitState::commit_unknown);
      } else {
        StageReceipt rejected;
        assert(stageBody(target,input,trialPlan,2049,&rejected)!=StageState::staged);
        assert(!rejected.valid);
      }
      assert((phase==0?disk.syncs:disk.reads)>=fail);
      disk.failRead=0; disk.failSync=0;
      FatVolume clean; assert(clean.begin(&disk)); SdFatFiles reader(clean);
      auto old=RecoverableDownloadStore(reader).validate(0);
      assert(old.state==SlotState::valid && old.generation==1 && old.length==2049);
    }
    printf("PASS actual FAT %s fault refuses success and preserves old generation: %u injections\n",
      phase==0?"staging device-sync":phase==1?"staging block-read":"seal block-read",count);
  }
}
