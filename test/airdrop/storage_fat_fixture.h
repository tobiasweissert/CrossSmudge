#pragma once
// Host qualification only. Actual pinned FatLib/common code uses this synthetic
// sparse block device; this does not model physical SD controller durability.
#include <openssl/sha.h>

#include <array>
#include <cassert>
#include <cstring>
#include <map>
#include <string>

#include "FatLib/FatFormatter.h"
#include "FatLib/FatVolume.h"
#include "StorageSink.h"

struct StorageDisk : FsBlockDevice {
  std::map<Sector_t, std::array<uint8_t, 512>> sectors;
  unsigned writes = 0, reads = 0, syncs = 0;
  unsigned failWrite = 0, failRead = 0, failSync = 0, prefix = 0;
  bool failed = false;
  bool isBusy() override { return false; }
  Sector_t sectorCount() override { return 32768; }
  bool syncDevice() override { const bool good = ++syncs != failSync; failed |= !good; return good; }
  bool readSector(Sector_t sector, uint8_t* bytes) override {
    if (sector >= sectorCount() || ++reads == failRead) { failed = true; return false; }
    const auto found = sectors.find(sector);
    if (found == sectors.end()) std::memset(bytes, 0, 512);
    else std::memcpy(bytes, found->second.data(), 512);
    return true;
  }
  bool readSectors(Sector_t sector, uint8_t* bytes, size_t count) override {
    for (size_t i = 0; i < count; ++i) if (!readSector(sector + i, bytes + i * 512)) return false;
    return true;
  }
  bool writeSector(Sector_t sector, const uint8_t* bytes) override {
    if (sector >= sectorCount()) return false;
    const bool good = ++writes != failWrite;
    if (good || prefix) std::memcpy(sectors[sector].data(), bytes, good ? 512 : prefix);
    failed |= !good;
    return good;
  }
  bool writeSectors(Sector_t sector, const uint8_t* bytes, size_t count) override {
    for (size_t i = 0; i < count; ++i) if (!writeSector(sector + i, bytes + i * 512)) return false;
    return true;
  }
};

struct StorageFatFiles : airdrop::storage::Files {
  FatVolume& volume;
  StorageDisk& disk;
  FatFile stage, reader;
  SHA256_CTX hash{};
  unsigned renames = 0, renameFirstWrite = 0, renameLastWrite = 0, afterFailedRename = 0;
  bool renameFailed = false;
  explicit StorageFatFiles(FatVolume& v, StorageDisk& d) : volume(v), disk(d) {}
  void operation() { if (renameFailed) ++afterFailedRename; }
  bool ready(uint64_t& freeBytes) override { operation(); freeBytes = 15000000; return true; }
  airdrop::storage::PathState stat(const char* path, uint32_t& size) override {
    operation(); using airdrop::storage::PathState;
    const std::string name(path); const auto slash = name.rfind('/');
    const std::string parent = slash == 0 ? "/" : name.substr(0, slash), base = name.substr(slash + 1);
    FatFile directory;
    if (!directory.open(&volume, parent.c_str(), O_RDONLY) || !directory.isDir()) { directory.close(); return PathState::Unreadable; }
    PathState result = PathState::Missing;
    for (unsigned i = 0; i < 8192; ++i) {
      FatFile child;
      if (!child.openNext(&directory)) { if (directory.getError()) result = PathState::Unreadable; child.close(); break; }
      char childName[256];
      const size_t n = child.getName(childName, sizeof childName);
      const bool same = n && !std::strcmp(childName, base.c_str());
      if (!n || n >= sizeof childName) result = PathState::Unreadable;
      else if (same) { result = child.isDir() ? PathState::Directory : PathState::File; size = child.fileSize(); }
      if (!child.close()) result = PathState::Unreadable;
      if (same || result == PathState::Unreadable) break;
    }
    if (!directory.close()) result = PathState::Unreadable;
    return result;
  }
  bool mkdirExclusive(const char* path) override { operation(); return volume.mkdir(path, false); }
  bool createStageExclusive(const char* path) override { operation(); return stage.open(&volume, path, O_CREAT | O_EXCL | O_RDWR); }
  size_t writeStage(const uint8_t* bytes, size_t length) override { operation(); assert(length <= 1024); return stage.write(bytes, length); }
  bool syncStage() override { operation(); return stage.sync(); }
  bool closeStage() override { operation(); return stage.close(); }
  bool writeRecordExclusive(const char* path, const uint8_t* bytes, size_t length) override {
    operation(); FatFile record;
    if (!record.open(&volume, path, O_CREAT | O_EXCL | O_RDWR)) return false;
    const bool written = record.write(bytes, length) == length;
    const bool synced = written && record.sync();
    const bool closed = record.close();
    return written && synced && closed;
  }
  bool openRead(const char* path, uint32_t& size) override {
    operation(); if (!reader.open(&volume, path, O_RDONLY) || reader.isDir()) { reader.close(); return false; }
    size = reader.fileSize(); return true;
  }
  int read(uint8_t* bytes, size_t length) override { operation(); return reader.read(bytes, length); }
  bool seek(uint32_t offset) override { operation(); return reader.seekSet(offset); }
  bool closeRead() override { operation(); return reader.close(); }
  bool renameExclusive(const char* oldPath, const char* newPath) override {
    operation(); ++renames; renameFirstWrite = disk.writes;
    const bool result = volume.rename(oldPath, newPath);
    renameLastWrite = disk.writes; renameFailed = !result; return result;
  }
  bool hashStart() override { operation(); return SHA256_Init(&hash) == 1; }
  bool hashUpdate(const uint8_t* bytes, size_t length) override { operation(); return SHA256_Update(&hash, bytes, length) == 1; }
  bool hashFinish(uint8_t digest[32]) override { operation(); return SHA256_Final(digest, &hash) == 1; }
  bool cancelled() const override { return false; }
};
