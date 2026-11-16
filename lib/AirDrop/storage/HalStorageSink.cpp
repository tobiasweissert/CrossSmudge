#include "HalStorageSink.h"

#if defined(FREEINK_DEVICE_STICKY) && FREEINK_DEVICE_STICKY && !defined(SIMULATOR)
#include <Logging.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

namespace airdrop::storage {
namespace {
bool equalName(const char* a, const char* b) {
  for (size_t i = 0;; ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
    if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
    if (x != y) return false;
    if (!x) return true;
  }
}
bool tokenName(const char* name) {
  if (strnlen(name, 33) != 32) return false;
  for (unsigned i = 0; i < 32; ++i)
    if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f'))) return false;
  return true;
}
}  // namespace
bool HalFiles::cancelled() const { return cancelRequested_.load(std::memory_order_acquire) || (deadlineMs_ && uint64_t(esp_timer_get_time()/1000) >= deadlineMs_); }
HalFiles::HalFiles() { mbedtls_sha256_init(&hash_); }
HalFiles::~HalFiles() {
  if (!closeStage()) LOG_ERR("AIRDROP", "SD stage close failed at owner destruction");
  if (!closeRead()) LOG_ERR("AIRDROP", "SD reader close failed at owner destruction");
  if (!inventory_.close()) LOG_ERR("AIRDROP", "SD inventory close failed at owner destruction");
  mbedtls_sha256_free(&hash_);
}
bool HalFiles::ready(uint64_t& freeBytes) {
  if (!Storage.ready()) return false;
  const uint64_t total = Storage.totalBytes(), used = Storage.usedBytes();
  if (!total || used > total) return false;
  freeBytes = total - used;
  return true;
}
PathState HalFiles::stat(const char* path, uint32_t& size) {
  size = 0;
  if (!path || path[0] != '/') return PathState::Unreadable;
  const size_t length = strnlen(path, sizeof parent_);
  const char* slash = std::strrchr(path, '/');
  if (length >= sizeof parent_ || !slash || !slash[1]) return PathState::Unreadable;
  const size_t parentLength = slash - path;
  if (parentLength) { std::memcpy(parent_, path, parentLength); parent_[parentLength] = 0; }
  else std::strcpy(parent_, "/");
  HalFile directory = Storage.open(parent_, O_RDONLY);
  if (!directory || !directory.isDirectory()) { LOG_ERR("AIRDROP", "Cannot scan SD parent %s for %s",parent_,path);directory.close(); return PathState::Unreadable; }
  PathState result = PathState::Missing;
  bool ended = false;
  for (unsigned count = 0; count < 8192; ++count) {
    if (cancelled()) { LOG_ERR("AIRDROP","SD scan cancelled: %s",path);result = PathState::Unreadable; ended = true; break; }
    HalFile child = directory.openNextFile();
    if (!child) {
      const bool healthy = !directory.iterationFailed() && !directory.allocationFailed();
      if(!healthy)LOG_ERR("AIRDROP","SD directory iteration failed: %s",parent_);
      child.close(); result = healthy ? PathState::Missing : PathState::Unreadable;
      ended = true; break;
    }
    const size_t n = child.getName(entryName_, sizeof entryName_);
    const bool matches = n && n < sizeof entryName_ && equalName(entryName_, slash + 1);
    if (!n || n >= sizeof entryName_) {LOG_ERR("AIRDROP","SD entry name unreadable in %s (length=%u)",parent_,unsigned(n));result = PathState::Unreadable;}
    else if (matches) {
      const uint64_t bytes = child.fileSize64();
      result = child.isDirectory() ? PathState::Directory : (bytes <= UINT32_MAX ? PathState::File : PathState::Unreadable);
      if (result == PathState::File) size = static_cast<uint32_t>(bytes);
    }
    if (!child.close()) {LOG_ERR("AIRDROP","SD entry close failed in %s",parent_);result = PathState::Unreadable;}
    if (matches || result == PathState::Unreadable) { ended = true; break; }
    vTaskDelay(1);
  }
  if (!ended) result = PathState::Unreadable;
  if (!directory.close()) {LOG_ERR("AIRDROP","SD parent close failed: %s",parent_);result = PathState::Unreadable;}
  LOG_INF("AIRDROP","SD path %s: state=%u",path,unsigned(result));
  return result;
}
bool HalFiles::mkdirExclusive(const char* path) {
  // SdFat mkdir returns false for an existing directory. Never recursively
  // create through an unvalidated sender path; core paths are locally derived.
  const bool ok=!cancelled() && Storage.mkdir(path, false);
  if(ok)LOG_INF("AIRDROP","SD mkdir %s: ok",path);
  else LOG_ERR("AIRDROP","SD mkdir failed: %s",path);
  return ok;
}
bool HalFiles::createStageExclusive(const char* path) {
  if (stage_ || cancelled()) return false;
  stage_ = Storage.open(path, O_CREAT | O_EXCL | O_RDWR);
  if (!stage_ || stage_.isDirectory()) { stage_.close(); return false; }
  return true;
}
size_t HalFiles::writeStage(const uint8_t* bytes, size_t length) {
  if (!stage_ || length > ChunkBytes || cancelled()) return 0;
  const size_t written = stage_.write(bytes, length);
  vTaskDelay(1); // Yield without holding the HAL's SPI/storage lock.
  return written;
}
bool HalFiles::syncStage() { return stage_ && stage_.sync(); }
bool HalFiles::closeStage() { return stage_.close(); }
bool HalFiles::writeRecordExclusive(const char* path, const uint8_t* bytes, size_t length) {
  if (length > ChunkBytes || cancelled()) return false;
  HalFile record = Storage.open(path, O_CREAT | O_EXCL | O_RDWR);
  if (!record || record.isDirectory()) { record.close(); return false; }
  const bool written = record.write(bytes, length) == length;
  const bool synced = written && record.sync();
  const bool closed = record.close();
  return written && synced && closed;
}
bool HalFiles::openRead(const char* path, uint32_t& size) {
  if (reader_ || cancelled()) return false;
  reader_ = Storage.open(path, O_RDONLY);
  if (!reader_ || reader_.isDirectory() || reader_.fileSize64() > UINT32_MAX) { reader_.close(); return false; }
  size = static_cast<uint32_t>(reader_.fileSize64());
  return true;
}
int HalFiles::read(uint8_t* bytes, size_t length) {
  if (!reader_ || length > ChunkBytes || cancelled()) return -1;
  const int n = reader_.read(bytes, length);
  vTaskDelay(1);
  return n;
}
bool HalFiles::seek(uint32_t offset) { return reader_ && !cancelled() && reader_.seek64(offset); }
bool HalFiles::closeRead() { return reader_.close(); }
bool HalFiles::renameExclusive(const char* oldPath, const char* newPath) {
  // Both pinned FAT and exFAT rename create destination with O_EXCL. A false
  // result remains uncertain; core performs no retry/removal or further write.
  return !cancelled() && Storage.rename(oldPath, newPath);
}
bool HalFiles::hashStart() { return mbedtls_sha256_starts(&hash_, 0) == 0; }
bool HalFiles::hashUpdate(const uint8_t* bytes, size_t length) { return mbedtls_sha256_update(&hash_, bytes, length) == 0; }
bool HalFiles::hashFinish(uint8_t digest[32]) { return mbedtls_sha256_finish(&hash_, digest) == 0; }

bool HalFiles::recoveryToken(unsigned index, char token[33], bool& scanOk) {
  scanOk = false;
  token[0] = 0;
  if (cancelled()) { inventory_.close(); return false; }
  if (index == 0) {
    if (!inventory_.close()) return false;
    inventoryIndex_ = inventoryEntries_ = 0;
    uint32_t ignored = 0;
    const auto inbox = stat("/Inbox", ignored);
    if (inbox == PathState::Missing) { scanOk = true; return false; }
    if (inbox != PathState::Directory) return false;
    const auto status = stat("/Inbox/.airdrop", ignored);
    if (status == PathState::Missing) { scanOk = true; return false; }
    if (status != PathState::Directory) return false;
    inventory_ = Storage.open("/Inbox/.airdrop", O_RDONLY);
    if (!inventory_ || !inventory_.isDirectory()) { inventory_.close(); return false; }
  } else if (index != inventoryIndex_ || !inventory_) {
    return false;
  }
  // Sequential calls retain one iterator, avoiding O(n^2) rescans. One extra
  // EOF probe distinguishes exactly 256 entries from an excessive inventory.
  while (inventoryEntries_ <= 256) {
    HalFile child = inventory_.openNextFile();
    if (!child) {
      scanOk = !inventory_.iterationFailed() && !inventory_.allocationFailed();
      child.close();
      if (!inventory_.close()) scanOk = false;
      return false;
    }
    if (++inventoryEntries_ > 256) { child.close(); inventory_.close(); return false; }
    const size_t length = child.getName(entryName_, sizeof entryName_);
    const bool valid = length == 32 && tokenName(entryName_) && child.isDirectory();
    const bool closed = child.close();
    if (!length || length >= sizeof entryName_ || !closed || !valid) { inventory_.close(); return false; }
    std::memcpy(token, entryName_, 33);
    ++inventoryIndex_;
    scanOk = true;
    return true;
  }
  inventory_.close();
  return false;
}
bool HalStorageSink::logged(bool result) {
  if (!result) LOG_ERR("AIRDROP", "SD receive failed: %s", sink_.error() ? sink_.error() : "unknown");
  return result;
}
bool HalStorageSink::begin(const char* name, uint32_t size) {
  if (sink_.state() == State::Receiving || sink_.state() == State::PublishAttempted) return logged(false);
  uint8_t nonce[16];
  esp_fill_random(nonce, sizeof nonce);
  return logged(sink_.begin(name, size, nonce));
}
bool HalStorageSink::append(const uint8_t* bytes, size_t length) { return logged(sink_.append(bytes, length)); }
bool HalStorageSink::finish() { return logged(sink_.finish()); }
void HalStorageSink::cancel() { files_.requestCancel(); sink_.cancel(); }
void HalStorageSink::abortTransfer() { sink_.cancel(); }
State HalStorageSink::inspect(const char* token) {
  const auto result = sink_.inspect(token);
  if (result == State::Uncertain || result == State::Failed) logged(false);
  return result;
}
State HalStorageSink::inspectJournal(const char* token) {
  const auto result = sink_.inspectJournal(token);
  if (result == State::Uncertain || result == State::Failed) logged(false);
  return result;
}
}  // namespace airdrop::storage
#endif
