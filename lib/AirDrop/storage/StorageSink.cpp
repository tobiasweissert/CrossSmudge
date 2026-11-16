#include "StorageSink.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../stream/StreamDecoder.h"

namespace airdrop::storage {
namespace {
constexpr uint32_t ReserveBytes = 1048576;
constexpr size_t RecordBytes = 512;
constexpr size_t RecordCrcOffset = 508;
constexpr size_t NameOffset = 64;
constexpr size_t FinalOffset = 185;
constexpr uint8_t Magic[8] = {'A', 'D', 'S', 'T', 'G', '0', '0', '1'};
uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t le32(const uint8_t* p) { return uint32_t(le16(p)) | (uint32_t(le16(p + 2)) << 16); }
void put32(uint8_t* p, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(value >> (8 * i));
}
bool tokenValid(const char* token) {
  if (!token || strnlen(token, 33) != 32) return false;
  for (unsigned i = 0; i < 32; ++i) if (!((token[i] >= '0' && token[i] <= '9') || (token[i] >= 'a' && token[i] <= 'f'))) return false;
  return true;
}
}  // namespace

uint32_t journalCrc32(const uint8_t* bytes, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

bool StorageSink::fail(const char* reason, bool uncertain) {
  error_ = reason;
  state_ = uncertain || publicationAttempted_ ? State::Uncertain : State::Failed;
  if (stageOpen_) {
    stageOpen_ = false;
    if (!files_.closeStage()) error_ = "SD stage close failed; retained for recovery";
  }
  return false;
}

bool StorageSink::paths(const char* token) {
  if (!tokenValid(token)) return fail("invalid recovery identity");
  std::memcpy(token_, token, sizeof token_);
  std::snprintf(sessionPath_, sizeof sessionPath_, "/Inbox/.airdrop/%s", token_);
  std::snprintf(stagePath_, sizeof stagePath_, "%s/payload.part", sessionPath_);
  return true;
}

bool StorageSink::ensureDirectory(const char* path) {
  uint32_t ignored = 0;
  const auto status = files_.stat(path, ignored);
  return status == PathState::Directory || (status == PathState::Missing && files_.mkdirExclusive(path));
}

bool StorageSink::begin(const char* name, uint32_t size, const uint8_t nonce[16]) {
  if (stageOpen_ || state_ == State::Receiving || state_ == State::PublishAttempted || state_ == State::Uncertain) {
    // Refusing another begin must not close the live writer or clear uncertainty.
    error_ = "previous receive session still owns storage";
    return false;
  }
  state_ = State::Idle;
  error_ = nullptr;
  publicationAttempted_ = false;
  if (files_.cancelled()) return fail("receive owner cancelled");
  written_ = 0;
  expected_ = size;
  finalPath_[0] = sessionPath_[0] = 0;
  std::memset(digest_, 0, sizeof digest_);
  const size_t length = name ? strnlen(name, sizeof name_) : 0;
  if (!nonce || !stream::safeName(name, length) || size > MaxPayloadBytes) return fail("unsupported filename or file size");
  std::memset(name_, 0, sizeof name_);
  std::memset(finalPath_, 0, sizeof finalPath_);
  std::memcpy(name_, name, length + 1);
  char token[33];
  static constexpr char Hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < 16; ++i) { token[2 * i] = Hex[nonce[i] >> 4]; token[2 * i + 1] = Hex[nonce[i] & 15]; }
  token[32] = 0;
  if (!paths(token)) return false;
  const char* dot = std::strrchr(name_, '.');
  if (!dot) dot = name_ + std::strlen(name_);
  const int stem = static_cast<int>(dot - name_);
  const int count = std::snprintf(finalPath_, sizeof finalPath_, "/Inbox/%.*s (AirDrop %s)%s", stem, name_, token_, dot);
  if (count < 0 || size_t(count) >= sizeof finalPath_) return fail("destination name exceeds storage bound");
  uint64_t available = 0;
  if (!files_.ready(available) || available < uint64_t(size) + ReserveBytes) return fail("SD unavailable or insufficient reserved free space");
  if (!ensureDirectory("/Inbox")) return fail("SD Inbox is unreadable or cannot be created");
  if (!ensureDirectory("/Inbox/.airdrop")) return fail("SD staging directory is unreadable or cannot be created");
  // mkdirExclusive refuses every prior session, including uncertain candidates.
  if (!files_.mkdirExclusive(sessionPath_)) return fail("cannot exclusively reserve a new receive session");
  if (!record(1)) return fail("receiving journal was not durably verified");
  if (!files_.createStageExclusive(stagePath_)) return fail("cannot exclusively create stage file");
  stageOpen_ = true;
  if (!files_.hashStart()) return fail("cannot initialize receive SHA256");
  state_ = State::Receiving;
  return true;
}

bool StorageSink::append(const uint8_t* bytes, size_t length) {
  if (state_ != State::Receiving || !stageOpen_) return fail("no writable accepted receive session");
  if (files_.cancelled()) { cancel(); return false; }
  if ((!bytes && length) || length > ChunkBytes || length > expected_ - written_) return fail("payload exceeds accepted size or chunk bound");
  if (!length) return true;
  const size_t written = files_.writeStage(bytes, length);
  if (written != length) {
    written_ += std::min(written, length);
    return fail("SD partial write; incomplete stage retained");
  }
  written_ += length;
  if (!files_.hashUpdate(bytes, length)) return fail("receive SHA256 update failed");
  return true;
}

void StorageSink::serialize(uint8_t phase) {
  std::memset(journal_, 0, sizeof journal_);
  std::memcpy(journal_, Magic, sizeof Magic);
  journal_[8] = 1;
  journal_[9] = phase;
  put32(journal_ + 12, expected_);
  if (phase > 1) std::memcpy(journal_ + 16, digest_, sizeof digest_);
  std::memcpy(journal_ + NameOffset, name_, std::strlen(name_) + 1);
  std::memcpy(journal_ + FinalOffset, finalPath_, std::strlen(finalPath_) + 1);
  std::memcpy(journal_ + 400, token_, 32);
  put32(journal_ + RecordCrcOffset, journalCrc32(journal_, RecordCrcOffset));
}

bool StorageSink::record(uint8_t phase) {
  serialize(phase);
  std::snprintf(recordPath_, sizeof recordPath_, "%s/phase.%u", sessionPath_, phase);
  if (!files_.writeRecordExclusive(recordPath_, journal_, RecordBytes)) return false;
  uint32_t size = 0;
  if (!files_.openRead(recordPath_, size)) return false;
  const bool read = size == RecordBytes && files_.read(buffer_, RecordBytes) == int(RecordBytes) &&
                    std::memcmp(buffer_, journal_, RecordBytes) == 0;
  const bool closed = files_.closeRead();
  return read && closed;
}

bool StorageSink::verify(const char* path) {
  uint32_t size = 0;
  if (!files_.openRead(path, size)) return false;
  bool valid = size == expected_ && files_.hashStart();
  uint32_t position = 0;
  while (valid && position < size) {
    if (files_.cancelled()) { valid = false; break; }
    const size_t length = std::min<size_t>(size - position, sizeof buffer_);
    valid = files_.read(buffer_, length) == int(length) && files_.hashUpdate(buffer_, length);
    position += length;
  }
  uint8_t digest[32];
  if (valid) valid = files_.hashFinish(digest) && std::memcmp(digest, digest_, sizeof digest) == 0;
  const bool closed = files_.closeRead();
  return valid && closed;
}

bool StorageSink::finish() {
  if (state_ != State::Receiving || !stageOpen_) return fail("no active complete receive stage");
  if (files_.cancelled()) { cancel(); return false; }
  if (written_ != expected_) return fail("received payload length differs from consent");
  if (!files_.hashFinish(digest_)) return fail("receive SHA256 finalization failed");
  if (!files_.syncStage()) return fail("SD stage sync failed");
  stageOpen_ = false;
  if (!files_.closeStage()) return fail("SD stage close failed");
  if (!verify(stagePath_)) return fail("SD stage readback/hash validation failed");
  if (!record(2)) return fail("complete-stage journal was not durably verified");
  state_ = State::CompleteStaged;
  uint32_t ignored = 0;
  if (files_.stat(finalPath_, ignored) != PathState::Missing) return fail("destination collision or unreadable Inbox; stage retained");
  publicationAttempted_ = true;
  state_ = State::PublishAttempted;
  if (!record(3)) return fail("publication intent could not be durably verified", true);
  if (files_.cancelled()) return fail("cancelled after publication intent; stage retained", true);
  // No write/remove/retry follows a failed rename. False may already alias the
  // old and new cluster chain; the intent record is the durable uncertain state.
  if (!files_.renameExclusive(stagePath_, finalPath_)) return fail("SD publication uncertain; retain both paths for repair", true);
  if (!verify(finalPath_) || files_.stat(stagePath_, ignored) != PathState::Missing)
    return fail("SD publication readback uncertain; retain recovery identity", true);
  if (!record(4)) return fail("SD publication confirmation uncertain", true);
  // A checked read/close can cross the owner deadline or observe UI cancel
  // after starting. Retain its durable receipt, but never expose late success.
  if (files_.cancelled()) return fail("cancelled after publication confirmation; retained receipt", true);
  state_ = State::PublishedVerified;
  error_ = nullptr;
  return true;
}

void StorageSink::cancel() {
  if (publicationAttempted_ || state_ == State::Uncertain || state_ == State::PublishedVerified) return;
  if (stageOpen_) {
    stageOpen_ = false;
    if (!files_.closeStage()) { fail("cancelled stage close uncertain"); return; }
  }
  state_ = State::Cancelled;
  error_ = "receive cancelled; incomplete stage retained";
}

bool StorageSink::loadRecord(uint8_t phase) {
  std::snprintf(recordPath_, sizeof recordPath_, "%s/phase.%u", sessionPath_, phase);
  uint32_t size = 0;
  if (!files_.openRead(recordPath_, size)) return false;
  const bool read = size == RecordBytes && files_.read(journal_, RecordBytes) == int(RecordBytes);
  const bool closed = files_.closeRead();
  if (!read || !closed || std::memcmp(journal_, Magic, sizeof Magic) || journal_[8] != 1 || journal_[9] != phase ||
      le32(journal_ + RecordCrcOffset) != journalCrc32(journal_, RecordCrcOffset) ||
      std::memcmp(journal_ + 400, token_, 32) || !std::memchr(journal_ + NameOffset, 0, sizeof name_) ||
      !std::memchr(journal_ + FinalOffset, 0, sizeof finalPath_)) return false;
  if (phase == 1) {
    expected_ = le32(journal_ + 12);
    if (expected_ > MaxPayloadBytes) return false;
    std::memcpy(name_, journal_ + NameOffset, sizeof name_);
    if (!stream::safeName(name_, std::strlen(name_))) return false;
    std::memcpy(finalPath_, journal_ + FinalOffset, sizeof finalPath_);
    // Never trust persisted arbitrary paths: reconstruct and compare identity.
    const char* dot = std::strrchr(name_, '.');
    if (!dot) dot = name_ + std::strlen(name_);
    const int n = std::snprintf(reinterpret_cast<char*>(buffer_), sizeof buffer_, "/Inbox/%.*s (AirDrop %s)%s",
                               int(dot - name_), name_, token_, dot);
    if (n < 0 || size_t(n) >= sizeof finalPath_ || std::strcmp(finalPath_, reinterpret_cast<char*>(buffer_))) return false;
  } else {
    if (le32(journal_ + 12) != expected_ || std::memcmp(journal_ + NameOffset, name_, sizeof name_) ||
        std::memcmp(journal_ + FinalOffset, finalPath_, sizeof finalPath_)) return false;
    if (phase == 2) std::memcpy(digest_, journal_ + 16, sizeof digest_);
    else if (std::memcmp(digest_, journal_ + 16, sizeof digest_)) return false;
  }
  return true;
}

State StorageSink::inspect(const char* token) { return inspectImpl(token, true); }
State StorageSink::inspectJournal(const char* token) { return inspectImpl(token, false); }

State StorageSink::inspectImpl(const char* token, const bool verifyPayload) {
  if (stageOpen_) { fail("cannot inspect recovery with an active stage", true); return state_; }
  // A read-only inventory must never clear a real uncertainty latch.
  if (state_ == State::Uncertain || state_ == State::PublishAttempted) return state_;
  publicationAttempted_ = false;
  error_ = nullptr;
  state_ = State::Uncertain;
  written_ = expected_ = 0;
  if (!paths(token)) return state_;
  uint64_t free = 0;
  if (!files_.ready(free) || !loadRecord(1)) { fail("recovery record unreadable or corrupt", true); return state_; }
  uint8_t last = 1;
  bool missing = false;
  for (uint8_t phase = 2; phase <= 4; ++phase) {
    std::snprintf(recordPath_, sizeof recordPath_, "%s/phase.%u", sessionPath_, phase);
    uint32_t size = 0;
    const auto status = files_.stat(recordPath_, size);
    if (status == PathState::Missing) { missing = true; continue; }
    if (missing || status != PathState::File || size != RecordBytes || !loadRecord(phase)) {
      fail("recovery phase is unreadable, incomplete or out of order", true); return state_;
    }
    last = phase;
  }
  if (last >= 3) publicationAttempted_ = true;
  if (last == 4) {
    uint32_t size = 0;
    if (verifyPayload && (!verify(finalPath_) || files_.stat(stagePath_, size) != PathState::Missing)) {
      fail("published candidate cannot be safely verified", true); return state_;
    }
    written_ = expected_;
    state_ = State::PublishedVerified;
  } else if (last == 3) {
    fail("publication intent exists; alias-safe SD repair required", true);
  } else if (last == 2) {
    state_ = !verifyPayload || verify(stagePath_) ? State::CompleteStaged : State::Uncertain;
  } else {
    uint32_t size = 0;
    const auto status = files_.stat(stagePath_, size);
    state_ = status == PathState::File && size <= expected_ ? State::RetainedPartial : State::Uncertain;
    // Historical phase 1 has no open writer; a distinct consent/nonce may begin.
    if (state_ == State::RetainedPartial) written_ = size;
  }
  if (state_ == State::Uncertain && !error_) error_ = "retained stage cannot be read safely";
  return state_;
}
}  // namespace airdrop::storage
