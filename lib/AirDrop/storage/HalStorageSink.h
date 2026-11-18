#pragma once

#include "StorageSink.h"

#if defined(FREEINK_DEVICE_STICKY) && FREEINK_DEVICE_STICKY && !defined(SIMULATOR)
#include <HalStorage.h>
#include <mbedtls/sha256.h>

#include <atomic>

namespace airdrop::storage {
// Single SD task owner. requestCancel is safe from the UI/main task; joining and
// destruction remain the receiver owner's responsibility. No API renders UI.
class HalFiles final : public Files {
 public:
  HalFiles();
  ~HalFiles() override;
  bool ready(uint64_t& freeBytes) override;
  PathState stat(const char* path, uint32_t& size) override;
  bool mkdirExclusive(const char* path) override;
  bool createStageExclusive(const char* path) override;
  size_t writeStage(const uint8_t* bytes, size_t length) override;
  bool syncStage() override;
  bool closeStage() override;
  bool writeRecordExclusive(const char* path, const uint8_t* bytes, size_t length) override;
  bool openRead(const char* path, uint32_t& size) override;
  int read(uint8_t* bytes, size_t length) override;
  bool seek(uint32_t offset) override;
  bool closeRead() override;
  bool renameExclusive(const char* oldPath, const char* newPath) override;
  bool hashStart() override;
  bool hashUpdate(const uint8_t* bytes, size_t length) override;
  bool hashFinish(uint8_t digest[32]) override;
  bool cancelled() const override;
  void setDeadlineMs(uint64_t deadline) { deadlineMs_ = deadline; }
  void requestCancel() { cancelRequested_.store(true, std::memory_order_release); }
  // Bounded healthy scan. Returns one pending session token, or false. scanOk
  // distinguishes a healthy empty/end directory from an unreadable SD card.
  bool recoveryToken(unsigned index, char token[33], bool& scanOk);

 private:
  // Same 1KiB I/O bound, but scheduling cost is independent of fragment size.
  // Yield outside HAL locks after 4KiB or 1ms of I/O work, whichever comes first.
  size_t ioBytesSinceYield_ = 0;
  int64_t ioLastYieldUs_ = 0;
  void cooperate(size_t bytes);
  HalFile stage_;
  HalFile reader_;
  HalFile inventory_;
  unsigned inventoryIndex_ = 0;
  unsigned inventoryEntries_ = 0;
  mbedtls_sha256_context hash_{};
  std::atomic<bool> cancelRequested_{false};
  uint64_t deadlineMs_ = 0; // Listener-owner only; UI cancellation remains atomic.
  // Directory paths/names are session storage, not >256-byte task locals.
  char parent_[PathBytes]{};
  char entryName_[256]{};
};

class HalStorageSink {
 public:
  HalStorageSink() : sink_(files_) {}
  bool begin(const char* name, uint32_t size);
  bool append(const uint8_t* bytes, size_t length);
  bool finish();
  // Cancellation is sticky for this owner lifetime; reentry uses a new joined
  // Session. UI/main task calls requestCancel; the listener calls cancel after observing
  // it, then joins before this object is destroyed.
  void requestCancel() { files_.requestCancel(); }
  void setDeadlineMs(uint64_t deadline) { files_.setDeadlineMs(deadline); }
  void cancel();
  void abortTransfer(); // Close only this stage; keep the receiver available for a new consent/nonce.
  State state() const { return sink_.state(); }
  const char* error() const { return sink_.error(); }
  const char* savedPath() const { return sink_.savedPath(); }
  const char* recoveryPath() const { return sink_.recoveryPath(); }
  uint32_t payloadBytes() const { return sink_.payloadBytes(); }
  uint32_t committedBytes() const { return sink_.committedBytes(); }
  State inspect(const char* token);
  State inspectJournal(const char* token);
  bool recoveryToken(unsigned index, char token[33], bool& scanOk) { return files_.recoveryToken(index, token, scanOk); }

 private:
  HalFiles files_;
  StorageSink sink_;
  bool logged(bool result);
};
}  // namespace airdrop::storage
#endif
