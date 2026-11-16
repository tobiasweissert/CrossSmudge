#include "storage_fat_fixture.h"

#include <algorithm>
#include <cstdio>
#include <memory>

using namespace airdrop::storage;
static constexpr uint8_t Token[16] = {1, 2, 3, 4, 5};
static constexpr char Hex[] = "01020304050000000000000000000000";
static bool transfer(StorageSink& sink) {
  if (!sink.begin("book.txt", 2049, Token)) return false;
  uint8_t bytes[128]; std::memset(bytes, 'a', sizeof bytes);
  for (size_t n = 0; n < 2049;) { const size_t count = std::min(sizeof bytes, size_t(2049 - n)); if (!sink.append(bytes, count)) return false; n += count; }
  return sink.finish();
}
static void oldIntact(StorageDisk& disk) {
  disk.failWrite = disk.failRead = disk.failSync = 0;
  auto owner = std::make_unique<FatVolume>(); auto& remounted = *owner; assert(remounted.begin(&disk, false));
  for (const char* path : {"/old.txt", "/apps/browser-save.dat", "/Inbox/existing.txt"}) {
    FatFile old; assert(old.open(&remounted, path, O_RDONLY)); char bytes[4]{};
    assert(old.fileSize() == 3 && old.read(bytes, 3) == 3 && !std::strcmp(bytes, "old")); assert(old.close());
  }
}
int main() {
  {
    StorageDisk blank;uint8_t scratch[512];FatFormatter format;assert(format.format(&blank,scratch));
    auto owner=std::make_unique<FatVolume>();assert(owner->begin(&blank,false));
    StorageFatFiles files(*owner,blank);StorageSink sink(files);assert(transfer(sink));
    std::puts("PASS first upload creates Inbox and hidden staging on an empty FAT volume");
  }
  StorageDisk disk; uint8_t sector[512]; FatFormatter formatter; assert(formatter.format(&disk, sector));
  {
    auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false)); assert(volume.mkdir("/apps")); assert(volume.mkdir("/Inbox"));
    for (const char* path : {"/old.txt", "/apps/browser-save.dat", "/Inbox/existing.txt"}) {
      FatFile old; assert(old.open(&volume, path, O_CREAT | O_EXCL | O_RDWR)); assert(old.write("old", 3) == 3); assert(old.sync()); assert(old.close());
    }
  }
  const auto baseline = disk.sectors;
  unsigned writes = 0, reads = 0, syncs = 0, renameFirst = 0, renameLast = 0;
  {
    auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false)); StorageFatFiles files(volume, disk); StorageSink sink(files);
    disk.writes = disk.reads = disk.syncs = 0;
    assert(transfer(sink)); writes = disk.writes; reads = disk.reads; syncs = disk.syncs;
    renameFirst = files.renameFirstWrite; renameLast = files.renameLastWrite;
    assert(files.renames == 1 && renameLast > renameFirst);
  }
  oldIntact(disk);
  {
    auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false));
    StorageFatFiles files(volume, disk); StorageSink recovered(files);
    assert(recovered.inspect(Hex) == State::PublishedVerified);
    FatFile saved; assert(saved.open(&volume, recovered.savedPath(), O_RDONLY));
    uint8_t bytes[128]; uint32_t count = 0;
    while (count < 2049) {
      const size_t n = std::min(sizeof bytes, size_t(2049 - count)); assert(saved.read(bytes, n) == int(n));
      for (size_t i = 0; i < n; ++i) assert(bytes[i] == 'a');
      count += n;
    }
    assert(saved.close());
  }
  disk.sectors = baseline;
  {
    auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false));
    FatFile existing; const char* path = "/Inbox/book (AirDrop 01020304050000000000000000000000).txt";
    assert(existing.open(&volume, path, O_CREAT | O_EXCL | O_RDWR)); assert(existing.write("old", 3) == 3);
    assert(existing.sync()); assert(existing.close());
    StorageFatFiles files(volume, disk); StorageSink sink(files); assert(!transfer(sink)); assert(files.renames == 0);
    assert(existing.open(&volume, path, O_RDONLY)); char content[4]{};
    assert(existing.read(content, 3) == 3 && !std::strcmp(content, "old")); assert(existing.close());
  }
  std::puts("PASS actual FAT remount/hash-confirmed publication and exclusive destination collision");
  unsigned writeCases = 0, uncertain = 0, trueAfterFault = 0, actualAliases = 0;
  for (unsigned fail = 1; fail <= writes; ++fail) for (unsigned prefix : {0u, 1u, 255u, 511u, 512u}) {
    disk.sectors = baseline; disk.failRead = disk.failSync = disk.failWrite = 0; disk.failed = false;
    bool uncertainRename = false;
    {
      auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false)); StorageFatFiles files(volume, disk); StorageSink sink(files);
      disk.writes = disk.reads = disk.syncs = 0; disk.failWrite = fail; disk.prefix = prefix;
      const bool success = transfer(sink);
      assert(disk.failed && files.renames <= 1 && files.afterFailedRename == 0);
      uncertainRename = files.renameFailed;
      if (files.renameFailed) { assert(!success && sink.state() == State::Uncertain); ++uncertain; }
      if (success) { assert(sink.state() == State::PublishedVerified); ++trueAfterFault; }
    }
    oldIntact(disk);
    if (uncertainRename) {
      auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false));
      FatFile stage, final;
      const bool a = stage.open(&volume, "/Inbox/.airdrop/01020304050000000000000000000000/payload.part", O_RDONLY);
      const bool b = final.open(&volume, "/Inbox/book (AirDrop 01020304050000000000000000000000).txt", O_RDONLY);
      if (a && b && stage.firstCluster() != 0 && stage.firstCluster() == final.firstCluster()) ++actualAliases;
      assert(stage.close()); assert(final.close());
    }
    if (fail > renameFirst) {
      auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false)); StorageFatFiles files(volume, disk); StorageSink recovered(files);
      const unsigned before = disk.writes;
      const State outcome = recovered.inspect(Hex);
      assert(disk.writes == before);
      assert(outcome == State::Uncertain || outcome == State::PublishedVerified || outcome == State::CompleteStaged);
      assert(files.renames == 0);
    }
    ++writeCases;
  }
  assert(actualAliases > 0);
  unsigned readCases = 0, syncCases = 0;
  for (bool reading : {false, true}) {
    const unsigned count = reading ? reads : syncs;
    for (unsigned fail = 1; fail <= count; ++fail) {
      disk.sectors = baseline; disk.failRead = disk.failSync = disk.failWrite = 0; disk.failed = false;
      {
        auto owner = std::make_unique<FatVolume>(); auto& volume = *owner; assert(volume.begin(&disk, false)); StorageFatFiles files(volume, disk); StorageSink sink(files);
        disk.writes = disk.reads = disk.syncs = 0;
        if (reading) disk.failRead = fail; else disk.failSync = fail;
        const bool success = transfer(sink);
        assert(disk.failed && files.afterFailedRename == 0 && files.renames <= 1);
        if (success) assert(sink.state() == State::PublishedVerified);
      }
      oldIntact(disk);
      if (reading) ++readCases; else ++syncCases;
    }
  }
  std::printf("PASS actual pinned FAT staging/publication partial-sector faults: %u writes; %u rename-uncertain; %u remounted shared-cluster aliases preserved; %u verified after transient lower-layer fault\n", writeCases, uncertain, actualAliases, trueAfterFault);
  std::printf("PASS actual FAT read/device-sync faults preserve existing files: %u reads, %u syncs; rename block writes %u..%u of %u\n", readCases, syncCases, renameFirst + 1, renameLast, writes);
}
