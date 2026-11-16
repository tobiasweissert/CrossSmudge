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
bool extension(const char* name, const char* suffix) {
  const size_t length = std::strlen(name), n = std::strlen(suffix);
  if (length < n) return false;
  for (size_t i = 0; i < n; ++i) {
    char c = name[length - n + i];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c != suffix[i]) return false;
  }
  return true;
}
struct Utf8 {
  uint32_t point = 0, minimum = 0;
  unsigned remaining = 0;
  bool feed(const uint8_t* bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) {
      const uint8_t c = bytes[i];
      if (remaining) {
        if ((c & 0xc0) != 0x80) return false;
        point = (point << 6) | (c & 0x3f);
        if (--remaining == 0 && (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))) return false;
      } else if (c < 0x80) {
        if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != '\f') || c == 0x7f) return false;
      } else if (c >= 0xc2 && c <= 0xdf) { point = c & 31; minimum = 0x80; remaining = 1; }
      else if (c >= 0xe0 && c <= 0xef) { point = c & 15; minimum = 0x800; remaining = 2; }
      else if (c >= 0xf0 && c <= 0xf4) { point = c & 7; minimum = 0x10000; remaining = 3; }
      else return false;
    }
    return true;
  }
};
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
  const int stem = static_cast<int>(dot - name_);
  const int count = std::snprintf(finalPath_, sizeof finalPath_, "/Inbox/%.*s (AirDrop %s)%s", stem, name_, token_, dot);
  if (count < 0 || size_t(count) >= sizeof finalPath_) return fail("destination name exceeds storage bound");
  text_ = extension(name_, ".txt");
  uint64_t available = 0;
  if (!files_.ready(available) || available < uint64_t(size) + ReserveBytes) return fail("SD unavailable or insufficient reserved free space");
  if (!ensureDirectory("/Inbox") || !ensureDirectory("/Inbox/.airdrop")) return fail("SD Inbox is unreadable or not a directory");
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

bool StorageSink::verify(const char* path, bool validateContent) {
  uint32_t size = 0;
  if (!files_.openRead(path, size)) return false;
  bool valid = size == expected_ && files_.hashStart();
  Utf8 utf8;
  uint32_t position = 0;
  while (valid && position < size) {
    if (files_.cancelled()) { valid = false; break; }
    const size_t length = std::min<size_t>(size - position, sizeof buffer_);
    valid = files_.read(buffer_, length) == int(length) && files_.hashUpdate(buffer_, length);
    if (valid && validateContent && text_) valid = utf8.feed(buffer_, length);
    position += length;
  }
  uint8_t digest[32];
  if (valid) valid = files_.hashFinish(digest) && std::memcmp(digest, digest_, sizeof digest) == 0;
  if (valid && validateContent) valid = text_ ? utf8.remaining == 0 : validateEpub();
  const bool closed = files_.closeRead();
  return valid && closed;
}

bool StorageSink::readAt(uint32_t offset, uint8_t* bytes, size_t length) {
  return !files_.cancelled() && offset <= expected_ && length <= expected_ - offset && files_.seek(offset) &&
         files_.read(bytes, length) == int(length);
}

bool StorageSink::validateEpub() {
  // OCF requires this first uncompressed, extra-field-free local member. This
  // validates ZIP structure only; no EPUB extraction or application writes.
  static constexpr uint8_t Mime[] = "application/epub+zip";
  uint8_t local[30];
  if (expected_ < 58 || !readAt(0, local, sizeof local) || le32(local) != 0x04034b50 || le16(local + 6) != 0 ||
      le16(local + 8) != 0 || le32(local + 14) != journalCrc32(Mime, 20) || le32(local + 18) != 20 ||
      le32(local + 22) != 20 || le16(local + 26) != 8 || le16(local + 28) != 0 ||
      !readAt(30, buffer_, 28) || std::memcmp(buffer_, "mimetype", 8) || std::memcmp(buffer_ + 8, Mime, 20)) return false;
  uint32_t eocdOffset = 0, centralOffset = 0, centralSize = 0;
  uint16_t entries = 0;
  bool found = false;
  unsigned candidates = 0;
  const uint32_t start = expected_ > 65557 ? expected_ - 65557 : 0;
  uint32_t end = expected_;
  uint8_t endRecord[22];
  while (!found && end > start) {
    const uint32_t begin = end - start > sizeof buffer_ ? end - sizeof buffer_ : start;
    const size_t length = end - begin;
    if (!readAt(begin, buffer_, length)) return false;
    if (length >= 4) for (size_t i = length - 3; i-- > 0;) {
      if (le32(buffer_ + i) != 0x06054b50) continue;
      if (++candidates > 32) return false;
      const uint32_t offset = begin + i;
      if (expected_ - offset < sizeof endRecord) continue;
      if (!readAt(offset, endRecord, sizeof endRecord)) return false;
      if (uint64_t(offset) + 22 + le16(endRecord + 20) != expected_) continue;
      if (le16(endRecord + 4) || le16(endRecord + 6) || le16(endRecord + 8) != le16(endRecord + 10)) return false;
      entries = le16(endRecord + 10);
      centralSize = le32(endRecord + 12); centralOffset = le32(endRecord + 16); eocdOffset = offset;
      if (!entries || entries > 4096 || centralSize > 2097152 || uint64_t(centralOffset) + centralSize != offset) return false;
      found = true; break;
    }
    if (begin == start) break;
    end = begin + 3; // Include signatures split across read blocks.
  }
  if (!found) return false;
  uint32_t position = centralOffset;
  bool mime = false, container = false, package = false;
  uint64_t expanded = 0;
  uint8_t central[46];
  for (uint16_t entry = 0; entry < entries; ++entry) {
    if (position > eocdOffset || eocdOffset - position < sizeof central || !readAt(position, central, sizeof central) ||
        le32(central) != 0x02014b50 || le16(central + 34)) return false;
    const uint16_t flags = le16(central + 8), method = le16(central + 10);
    const uint16_t name = le16(central + 28), extra = le16(central + 30), comment = le16(central + 32);
    const uint32_t compressed = le32(central + 20), uncompressed = le32(central + 24), offset = le32(central + 42);
    if ((flags & 0x2041) || (method != 0 && method != 8) || !name || name > 512 ||
        uint64_t(position) + 46 + name + extra + comment > eocdOffset ||
        uint64_t(offset) + 30 > centralOffset || uint64_t(expanded) + uncompressed > 268435456 ||
        uncompressed > 67108864 || !readAt(position + 46, buffer_, name) || std::memchr(buffer_, 0, name)) return false;
    expanded += uncompressed;
    if (buffer_[0] == '/' || std::memchr(buffer_, '\\', name)) return false;
    // ZIP paths remain inside the opaque EPUB. Reject traversal/special paths
    // even though nothing is extracted by this feature.
    size_t component = 0;
    for (size_t i = 0; i <= name; ++i) if (i == name || buffer_[i] == '/') {
      const size_t n = i - component;
      if ((n == 2 && buffer_[component] == '.' && buffer_[component + 1] == '.') ||
          (n == 1 && buffer_[component] == '.') || (n == 0 && i != name)) return false;
      component = i + 1;
    }
    if (!readAt(offset, local, sizeof local) || le32(local) != 0x04034b50 || le16(local + 6) != flags ||
        le16(local + 8) != method || le16(local + 26) != name ||
        uint64_t(offset) + 30 + name + le16(local + 28) + compressed > centralOffset ||
        !readAt(offset + 30, buffer_ + 512, name) || std::memcmp(buffer_, buffer_ + 512, name)) return false;
    if (!(flags & 8) && (le32(local + 14) != le32(central + 16) || le32(local + 18) != compressed ||
                         le32(local + 22) != uncompressed)) return false;
    if (name == 8 && !std::memcmp(buffer_, "mimetype", 8)) {
      if (mime || offset || method || flags || compressed != 20 || uncompressed != 20) return false;
      mime = true;
    }
    if (name == 22 && !std::memcmp(buffer_, "META-INF/container.xml", 22)) container = uncompressed != 0;
    if (name > 4 && !std::memcmp(buffer_ + name - 4, ".opf", 4)) package = uncompressed != 0;
    position += 46 + name + extra + comment;
  }
  return position == eocdOffset && mime && container && package;
}

bool StorageSink::finish() {
  if (state_ != State::Receiving || !stageOpen_) return fail("no active complete receive stage");
  if (files_.cancelled()) { cancel(); return false; }
  if (written_ != expected_) return fail("received payload length differs from consent");
  if (!files_.hashFinish(digest_)) return fail("receive SHA256 finalization failed");
  if (!files_.syncStage()) return fail("SD stage sync failed");
  stageOpen_ = false;
  if (!files_.closeStage()) return fail("SD stage close failed");
  if (!verify(stagePath_, true)) return fail("SD stage readback/hash/content validation failed");
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
  if (!verify(finalPath_, false) || files_.stat(stagePath_, ignored) != PathState::Missing)
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
    const int n = std::snprintf(reinterpret_cast<char*>(buffer_), sizeof buffer_, "/Inbox/%.*s (AirDrop %s)%s",
                               int(dot - name_), name_, token_, dot);
    if (n < 0 || size_t(n) >= sizeof finalPath_ || std::strcmp(finalPath_, reinterpret_cast<char*>(buffer_))) return false;
    text_ = extension(name_, ".txt");
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
    if (verifyPayload && (!verify(finalPath_, true) || files_.stat(stagePath_, size) != PathState::Missing)) {
      fail("published candidate cannot be safely verified", true); return state_;
    }
    written_ = expected_;
    state_ = State::PublishedVerified;
  } else if (last == 3) {
    fail("publication intent exists; alias-safe SD repair required", true);
  } else if (last == 2) {
    state_ = !verifyPayload || verify(stagePath_, true) ? State::CompleteStaged : State::Uncertain;
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
