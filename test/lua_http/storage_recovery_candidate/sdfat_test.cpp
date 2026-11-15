#include "SdFatFixture.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace recoverable;
struct Input : BodyInput {
  bool read(uint32_t o, uint8_t* p, size_t n) override {
    for (size_t i=0;i<n;++i) p[i]=uint8_t(o+i+17);
    return true;
  }
  bool cancelled() override { return false; }
};
StageReceipt stage(SdFatFiles& f) {
  Input input; StageReceipt receipt;
  auto plan=admitWrite(RecoverableDownloadStore(f).recover(),2049,false);
  assert(plan.state==AdmissionState::ready);
  assert(stageBody(f,input,plan,2049,&receipt)==StageState::staged);
  return receipt;
}
int main() {
  BlockDisk disk; uint8_t sector[512]; FatFormatter formatter;
  assert(formatter.format(&disk,sector));
  FatVolume volume; assert(volume.begin(&disk));
  for(const char* name : {"slot0.dat","slot1.dat"}) {
    FatFile file; assert(file.open(&volume,name,O_CREAT|O_RDWR)); assert(file.close());
  }
  SdFatFiles files(volume); Input input;
  auto first=stage(files); assert(sealStaged(files,input,first)==CommitState::committed);
  // sealStaged has synchronized and closed its writer before this snapshot.
  const auto baseline=disk.sectors;
  auto second=stage(files); const auto staged=disk.sectors;
  disk.writes=0;
  assert(sealStaged(files,input,second)==CommitState::committed);
  const unsigned writes=disk.writes; assert(writes>0);
  puts("PASS actual pinned FAT format stage seal and readback");
  unsigned injected=0;
  for(unsigned fail=1;fail<=writes;++fail) for(bool mutate : {false,true}) {
    disk.sectors=staged; disk.writes=0; disk.failAt=0;
    FatVolume trialVolume; assert(trialVolume.begin(&disk)); SdFatFiles trial(trialVolume);
    disk.failAt=fail; disk.mutateOnFailure=mutate;
    assert(sealStaged(trial,input,second)==CommitState::commit_unknown);
    assert(disk.writes>=fail); ++injected;
    disk.failAt=0;
    // Simulate power-cycle: do not close/sync failed writer or reuse its FAT cache.
    FatVolume reopened; assert(reopened.begin(&disk)); SdFatFiles reader(reopened);
    auto result=RecoverableDownloadStore(reader).recover();
    assert(result.state==RecoveryState::selected);
    assert(result.record.generation==1 || result.record.generation==2);
    auto old=RecoverableDownloadStore(reader).validate(0);
    assert(old.state==SlotState::valid && old.generation==1 && old.length==2049);
    std::vector<uint8_t> bytes(2049); assert(reader.read(0,24,bytes.data(),bytes.size()));
    for(size_t i=0;i<bytes.size();++i) assert(bytes[i]==uint8_t(i+17));
  }
  assert(injected==writes*2);
  printf("PASS every seal block-write fault before and after mutation recovers old bytes (%u injections)\n",injected);
  disk.sectors=baseline; disk.failAt=0;
  FatVolume finalVolume; assert(finalVolume.begin(&disk)); SdFatFiles finalReader(finalVolume);
  assert(RecoverableDownloadStore(finalReader).recover().record.generation==1);
  puts("PASS independent remount reads baseline without stale writer cache");
}
