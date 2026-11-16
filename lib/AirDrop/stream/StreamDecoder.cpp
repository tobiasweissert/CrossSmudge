#include "StreamDecoder.h"

#include <algorithm>
#include <cstring>

namespace airdrop::stream {
namespace {
static_assert(TINFL_LZ_DICT_SIZE == 32768, "AirDrop requires the admitted 32 KiB history window");
constexpr uint32_t kTypeMask = 0170000;
constexpr uint32_t kRegular = 0100000;
constexpr uint32_t kDirectory = 0040000;
constexpr uint32_t kMaxMetadata = 1000000;
constexpr uint32_t kMaxSidecar = 65536;
constexpr uint32_t kMaxEntries = 16;
constexpr size_t kWriteChunk = 1024;

bool octal(const uint8_t* in, size_t n, uint32_t& out) {
  out = 0;
  for (size_t i = 0; i < n; ++i) {
    if (in[i] < '0' || in[i] > '7') return false;
    const uint32_t digit = in[i] - '0';
    if (out > (UINT32_MAX - digit) / 8) return false;
    out = out * 8 + digit;
  }
  return true;
}

// Validate all numeric header fields, including the 33-bit odc mtime. File sizes
// are separately constrained to uint32_t and the accepted manifest.
bool odcDigits(const uint8_t* in, size_t n) {
  for (size_t i = 0; i < n; ++i) if (in[i] < '0' || in[i] > '7') return false;
  return true;
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
bool insensitive(const char* s, size_t n, const char* expected) {
  if (std::strlen(expected) != n) return false;
  for (size_t i = 0; i < n; ++i) if (lower(s[i]) != expected[i]) return false;
  return true;
}

bool safeUtf8(const char* s, size_t n) {
  for (size_t i = 0; i < n;) {
    const auto first = static_cast<uint8_t>(s[i++]);
    uint32_t point = first;
    size_t extra = 0;
    uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { point &= 0x1f; extra = 1; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { point &= 0x0f; extra = 2; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { point &= 7; extra = 3; minimum = 0x10000; }
    else if (first >= 0x80) return false;
    if (extra > n - i) return false;
    for (size_t j = 0; j < extra; ++j) {
      const auto c = static_cast<uint8_t>(s[i++]);
      if ((c & 0xc0) != 0x80) return false;
      point = (point << 6) | (c & 0x3f);
    }
    if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return false;
    if (point < 0x20 || (point >= 0x7f && point <= 0x9f) || point == 0x200e || point == 0x200f ||
        (point >= 0x202a && point <= 0x202e) || (point >= 0x2066 && point <= 0x2069)) return false;
  }
  return true;
}
}  // namespace

bool safeName(const char* name, size_t length) {
  if (!name || length == 0 || length > kMaxName || name[0] == '.' || name[length - 1] == '.' ||
      name[length - 1] == ' ' || !safeUtf8(name, length)) return false;
  size_t stem = length;
  for (size_t i = 0; i < length; ++i) {
    const char c = name[i];
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
      return false;
    if (c == '.' && stem == length) stem = i;
  }
  if (!stem || name[stem - 1] == ' ' || name[stem - 1] == '.') return false;
  if (insensitive(name, stem, "con") || insensitive(name, stem, "prn") || insensitive(name, stem, "aux") ||
      insensitive(name, stem, "nul")) return false;
  if (stem == 4 && (insensitive(name, 3, "com") || insensitive(name, 3, "lpt")) && name[3] >= '1' && name[3] <= '9')
    return false;
  return (length > 4 && insensitive(name + length - 4, 4, ".txt")) ||
         (length > 5 && insensitive(name + length - 5, 5, ".epub"));
}

Status Decoder::fail(const char* reason) {
  error_ = reason;
  initialized_ = false;
  return Status::Error;
}

bool Decoder::cancelled() const { return sink_.cancelled && sink_.cancelled(sink_.context); }

bool Decoder::reset(const Manifest& manifest, const Sink sink) {
  // Reset scalar state without a second large temporary on the task stack.
  manifest_ = manifest;
  sink_ = sink;
  error_ = nullptr;
  initialized_ = false;
  done_ = dvzipEnd_ = fileSeen_ = rootSeen_ = sidecarSeen_ = currentFile_ = false;
  compressed_ = inflateDone_ = false;
  recordHeaderBytes_ = 0;
  recordRemaining_ = recordExpanded_ = records_ = encodedBytes_ = archiveBytes_ = payloadBytes_ = 0;
  metadataBytes_ = entries_ = headerBytes_ = nameBytes_ = nameSize_ = memberSize_ = memberRemaining_ = 0;
  memberMode_ = memberLinks_ = trailerPadding_ = 0;
  windowPosition_ = pendingStart_ = pendingBytes_ = 0;
  cpioState_ = CpioState::Header;
  const size_t nameLength = strnlen(manifest_.name, sizeof manifest_.name);
  const size_t pathLength = strnlen(manifest_.archivePath, sizeof manifest_.archivePath);
  const char* pathName = manifest_.archivePath;
  if (pathName[0] == '.' && pathName[1] == '/') pathName += 2;
  if (!sink.append || nameLength == sizeof manifest_.name || !safeName(manifest_.name, nameLength) ||
      manifest_.size > kMaxPayload || pathLength == sizeof manifest_.archivePath ||
      std::strcmp(pathName, manifest_.name) != 0) {
    fail("invalid accepted manifest or sink");
    return false;
  }
  initialized_ = true;
  return true;
}

bool Decoder::parseHeader() {
  if (std::memcmp(cpioHeader_, "070707", 6) || !odcDigits(cpioHeader_ + 6, 70) || ++entries_ > kMaxEntries) {
    fail("unsupported or invalid odc header"); return false;
  }
  uint32_t rdev = 0;
  if (!octal(cpioHeader_ + 18, 6, memberMode_) || !octal(cpioHeader_ + 36, 6, memberLinks_) ||
      !octal(cpioHeader_ + 42, 6, rdev) || !octal(cpioHeader_ + 59, 6, nameSize_) ||
      !octal(cpioHeader_ + 65, 11, memberSize_) || nameSize_ == 0 || nameSize_ > sizeof memberName_ ||
      memberSize_ > kMaxPayload || memberMode_ > 0177777 || rdev != 0) {
    fail("odc fields exceed policy"); return false;
  }
  nameBytes_ = 0;
  cpioState_ = CpioState::Name;
  return true;
}

void Decoder::endMember() {
  if (currentFile_) fileSeen_ = true;
  currentFile_ = false;
  headerBytes_ = 0;
  cpioState_ = CpioState::Header;
}

bool Decoder::admitName() {
  if (memberName_[nameSize_ - 1] != 0 || std::memchr(memberName_, 0, nameSize_ - 1)) {
    fail("invalid member name termination"); return false;
  }
  const char* name = memberName_;
  if (!std::strcmp(name, "TRAILER!!!")) {
    if (memberSize_ || !fileSeen_ || memberMode_ != 0 || memberLinks_ != 1) {
      fail("invalid or premature trailer"); return false;
    }
    cpioState_ = CpioState::Trailer;
    return true;
  }
  if (!std::strcmp(name, ".")) {
    if (rootSeen_ || (memberMode_ & kTypeMask) != kDirectory || memberSize_ ||
        (memberLinks_ != 1 && memberLinks_ != 2)) {
      fail("invalid root wrapper"); return false;
    }
    rootSeen_ = true;
    endMember();
    return true;
  }
  if (name[0] == '.' && name[1] == '/') name += 2;
  if ((memberMode_ & kTypeMask) != kRegular || memberLinks_ != 1 || (memberMode_ & 07111)) {
    fail("nonregular, linked or executable member"); return false;
  }
  currentFile_ = false;
  if (!std::strcmp(memberName_, manifest_.archivePath)) {
    if (fileSeen_ || memberSize_ != manifest_.size) { fail("member does not match accepted manifest"); return false; }
    currentFile_ = true;
  } else if (name[0] == '.' && name[1] == '_' && !std::strcmp(name + 2, manifest_.name) &&
             ((memberName_[0] == '.' && memberName_[1] == '/') ==
              (manifest_.archivePath[0] == '.' && manifest_.archivePath[1] == '/'))) {
    if (sidecarSeen_ || memberSize_ > kMaxSidecar) { fail("AppleDouble quota or duplicate"); return false; }
    sidecarSeen_ = true;
  } else { fail("unexpected member path"); return false; }
  memberRemaining_ = memberSize_;
  cpioState_ = CpioState::Body;
  if (!memberRemaining_) endMember();
  return true;
}

Status Decoder::cpio(const uint8_t* bytes, const size_t length, size_t& consumed) {
  consumed = 0;
  while (consumed < length) {
    if (cancelled()) return fail("cancelled");
    if (archiveBytes_ >= kMaxArchive) return fail("archive byte quota");
    if (cpioState_ == CpioState::Body && currentFile_) {
      size_t n = std::min({length - consumed, size_t(memberRemaining_), kWriteChunk,
                           size_t(kMaxArchive - archiveBytes_)});
      if (n > kMaxPayload - payloadBytes_) return fail("payload byte quota");
      const auto result = sink_.append(sink_.context, bytes + consumed, n);
      if (result == WriteResult::WouldBlock) return Status::WouldBlock;
      if (result != WriteResult::Consumed) return fail("storage sink refused payload");
      consumed += n;
      archiveBytes_ += n;
      payloadBytes_ += n;
      memberRemaining_ -= n;
      if (!memberRemaining_) endMember();
      continue;
    }
    const uint8_t byte = bytes[consumed++];
    ++archiveBytes_;
    if (++metadataBytes_ > kMaxMetadata) return fail("archive metadata byte quota");
    switch (cpioState_) {
      case CpioState::Header:
        cpioHeader_[headerBytes_++] = byte;
        if (headerBytes_ == sizeof cpioHeader_ && !parseHeader()) return Status::Error;
        break;
      case CpioState::Name:
        memberName_[nameBytes_++] = static_cast<char>(byte);
        if (nameBytes_ == nameSize_ && !admitName()) return Status::Error;
        break;
      case CpioState::Body:
        if (--memberRemaining_ == 0) endMember();
        break;
      case CpioState::Trailer:
        if (byte != 0 || ++trailerPadding_ > 511) return fail("trailing archive data");
        break;
    }
  }
  return Status::Progress;
}

Status Decoder::drain() {
  while (pendingBytes_) {
    size_t used = 0;
    const auto result = cpio(window_ + pendingStart_, pendingBytes_, used);
    pendingStart_ += used;
    pendingBytes_ -= used;
    if (result != Status::Progress) return result;
  }
  return Status::Progress;
}

Status Decoder::feed(const uint8_t* input, const size_t length, size_t& consumed) {
  consumed = 0;
  if (!initialized_) return Status::Error;
  if (cancelled()) return fail("cancelled");
  if (!input && length) return fail("null input");
  if (done_) return length ? fail("data after finish") : Status::Complete;
  for (;;) {
    const auto drained = drain();
    if (drained != Status::Progress) return drained;
    if (recordRemaining_ == 0 && recordHeaderBytes_ == 4) {
      if (compressed_ && !inflateDone_) return fail("truncated compressed record");
      recordHeaderBytes_ = 0;
    }
    if (consumed == length) return Status::Progress;
    if (dvzipEnd_) return fail("data after DVZip end record");
    if (encodedBytes_ >= kMaxEncoded) return fail("encoded byte quota");
    if (recordHeaderBytes_ < 4) {
      recordHeader_[recordHeaderBytes_++] = input[consumed++];
      ++encodedBytes_;
      if (recordHeaderBytes_ != 4) continue;
      const uint32_t word = (uint32_t(recordHeader_[0]) << 24) | (uint32_t(recordHeader_[1]) << 16) |
                            (uint32_t(recordHeader_[2]) << 8) | recordHeader_[3];
      if (!word) {
        dvzipEnd_ = true;
        recordHeaderBytes_ = 0;
        continue;
      }
      recordRemaining_ = word & 0x7fffffffu;
      if (!recordRemaining_ || recordRemaining_ > kMaxRecord || ++records_ > kMaxRecords)
        return fail("DVZip record quota or invalid zero stored record");
      compressed_ = !(word & 0x80000000u);
      inflateDone_ = false;
      recordExpanded_ = 0;
      windowPosition_ = 0;
      if (compressed_) tinfl_init(&inflater_);
      continue;
    }
    size_t available = std::min({length - consumed, size_t(recordRemaining_), size_t(kMaxEncoded - encodedBytes_)});
    if (!available) return fail("encoded byte quota");
    if (!compressed_) {
      size_t used = 0;
      const auto result = cpio(input + consumed, available, used);
      consumed += used;
      encodedBytes_ += used;
      recordRemaining_ -= used;
      if (result != Status::Progress) return result;
      continue;
    }
    size_t produced = sizeof window_ - windowPosition_;
    size_t used = available;
    const uint32_t flags = TINFL_FLAG_PARSE_ZLIB_HEADER |
                           (available < recordRemaining_ ? TINFL_FLAG_HAS_MORE_INPUT : 0);
    const auto status = tinfl_decompress(&inflater_, input + consumed, &used, window_,
                                         window_ + windowPosition_, &produced, flags);
    consumed += used;
    encodedBytes_ += used;
    recordRemaining_ -= used;
    if (status < TINFL_STATUS_DONE) return fail("invalid zlib record or checksum");
    if (produced > kMaxRecord - recordExpanded_) return fail("expanded record quota");
    recordExpanded_ += produced;
    pendingStart_ = windowPosition_;
    pendingBytes_ = produced;
    windowPosition_ += produced;
    if (windowPosition_ == sizeof window_) windowPosition_ = 0;
    if (status == TINFL_STATUS_DONE) {
      inflateDone_ = true;
      if (recordRemaining_) return fail("trailing compressed record data");
    } else if (status == TINFL_STATUS_NEEDS_MORE_INPUT && !recordRemaining_) {
      return fail("truncated zlib record");
    }
    if (!used && !produced) return fail("inflater made no progress");
  }
}

Status Decoder::finish() {
  if (!initialized_) return Status::Error;
  if (cancelled()) return fail("cancelled");
  const auto drained = drain();
  if (drained != Status::Progress) return drained;
  if (recordRemaining_ || recordHeaderBytes_ || (compressed_ && !inflateDone_) ||
      cpioState_ != CpioState::Trailer || !fileSeen_ || payloadBytes_ != manifest_.size)
    return fail("incomplete DVZip/archive/manifest");
  done_ = true;
  return Status::Complete;
}
}  // namespace airdrop::stream
