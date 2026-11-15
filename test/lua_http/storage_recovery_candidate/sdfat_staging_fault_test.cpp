// Qualification-only: actual FAT implementation, synthetic block failures.
#include "SdFatFixture.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace recoverable;
struct FaultDisk : BlockDisk {
  unsigned prefix = 512;
  bool writeSector(Sector_t s, const uint8_t* p) override {
    if (s >= sectorCount()) return false;
    bool bad = ++writes == failAt;
    if (!bad) std::memcpy(sectors[s].data(), p, 512);
    else if (mutateOnFailure) std::memcpy(sectors[s].data(), p, prefix);
    return !bad;
  }
};
struct Bytes : BodyInput {
  bool read(uint32_t o, uint8_t* p, size_t n) override {
    for(size_t i=0;i<n;++i) p[i]=uint8_t(o+i+17);
    return true;
  }
  bool cancelled() override { return false; }
};
void oldIntact(FaultDisk& disk) {
  disk.failAt=0;
  FatVolume volume; assert(volume.begin(&disk)); SdFatFiles files(volume);
  auto old=RecoverableDownloadStore(files).validate(0);
  assert(old.state==SlotState::valid && old.generation==1 && old.length==2049);
  std::vector<uint8_t> bytes(2049); assert(files.read(0,24,bytes.data(),bytes.size()));
  for(size_t i=0;i<bytes.size();++i) assert(bytes[i]==uint8_t(i+17));
  assert(RecoverableDownloadStore(files).recover().state==RecoveryState::selected);
}
int main() {
  FaultDisk disk; uint8_t sector[512]; FatFormatter formatter;
  assert(formatter.format(&disk,sector));
  FatVolume initial; assert(initial.begin(&disk));
  for(const char* name : {"slot0.dat","slot1.dat"}) {
    FatFile f; assert(f.open(&initial,name,O_CREAT|O_RDWR)); assert(f.close());
  }
  Bytes input; SdFatFiles files(initial); StageReceipt receipt;
  auto plan=admitWrite(RecoverableDownloadStore(files).recover(),2049,false);
  assert(stageBody(files,input,plan,2049,&receipt)==StageState::staged);
  assert(sealStaged(files,input,receipt)==CommitState::committed);
  // Populate both slots: next reset must truncate an existing old generation,
  // not merely create a file in an empty test directory.
  plan=admitWrite(RecoverableDownloadStore(files).recover(),2049,false);
  assert(stageBody(files,input,plan,2049,&receipt)==StageState::staged);
  assert(sealStaged(files,input,receipt)==CommitState::committed);
  // Keep generation 1 selected for preservation assertions by corrupting only
  // inactive generation 2 header. Real filesystem writes are synchronized.
  assert(files.openSeal(1)); uint8_t zero[24]={};
  assert(files.write(1,0,zero,sizeof(zero))); assert(files.sync(1)); assert(files.close(1));
  const auto baseline=disk.sectors;
  plan=admitWrite(RecoverableDownloadStore(files).recover(),2049,false);
  assert(plan.state==AdmissionState::ready && plan.slot==1);
  disk.writes=0;
  assert(stageBody(files,input,plan,2049,&receipt)==StageState::staged);
  unsigned stageWrites=disk.writes; assert(stageWrites>0);
  const auto staged=disk.sectors;
  disk.writes=0; assert(sealStaged(files,input,receipt)==CommitState::committed);
  unsigned sealWrites=disk.writes; assert(sealWrites>0);
  unsigned stageCases=0,sealCases=0;
  for(bool sealing : {false,true}) {
    const unsigned count=sealing?sealWrites:stageWrites;
    for(unsigned fail=1;fail<=count;++fail) for(unsigned prefix : {0U,1U,23U,24U,255U,511U,512U}) {
      disk.sectors=sealing?staged:baseline; disk.writes=0; disk.failAt=0;
      FatVolume trialVolume; assert(trialVolume.begin(&disk)); SdFatFiles trial(trialVolume);
      disk.prefix=prefix; disk.mutateOnFailure=true; disk.failAt=fail;
      if(sealing) {
        assert(sealStaged(trial,input,receipt)==CommitState::commit_unknown);
        ++sealCases;
      } else {
        StageReceipt rejected; rejected.valid=true;
        assert(stageBody(trial,input,plan,2049,&rejected)==StageState::io_error);
        assert(!rejected.valid); ++stageCases;
      }
      assert(disk.writes>=fail);
      // New volume and handles abandon the former cache, no retry publication.
      oldIntact(disk);
    }
  }
  printf("PASS actual FAT staging/reset/sync block writes preserve prior slot: %u faults across %u writes\n",stageCases,stageWrites);
  printf("PASS actual FAT seal partial-sector failures preserve prior slot: %u faults across %u writes\n",sealCases,sealWrites);
  assert(stageCases==stageWrites*7 && sealCases==sealWrites*7);
}
