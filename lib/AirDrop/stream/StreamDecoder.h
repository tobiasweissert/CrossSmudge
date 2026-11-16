#pragma once

#include <cstddef>
#include <cstdint>

#include <MinizConfig.h>

namespace airdrop::stream {

constexpr uint32_t kMaxPayload = 30000000;
constexpr uint32_t kMaxArchive = 32000000;
constexpr uint32_t kMaxEncoded = 40000000;
constexpr uint32_t kMaxRecord = 1048576;
constexpr uint32_t kMaxRecords = 16384;
constexpr size_t kMaxName = 120;

enum class WriteResult { Consumed, WouldBlock, Error };
enum class Status { Progress, WouldBlock, Complete, Error };

struct Manifest {
  char name[kMaxName + 1]{};
  // Exact consented CPIO path: basename or ./basename. No directory wrappers.
  char archivePath[244]{};
  uint32_t size = 0;
};

// append accepts the entire borrowed span or none; spans are at most 1024 bytes.
// WouldBlock retains all unaccepted data in the decoder/caller; retry feed with
// exactly the unconsumed input. The owner applies time budgets and queue waits.
struct Sink {
  void* context = nullptr;
  WriteResult (*append)(void*, const uint8_t*, size_t) = nullptr;
  bool (*cancelled)(void*) = nullptr;
};

// Caller owns this fixed workspace for the receive session. It is deliberately
// too large for a task stack: the S3 owner must allocate it once in admitted
// PSRAM, then release after the listener/sink joins. No allocation, framebuffer
// loan, SD operation, UI callback, or publication occurs inside this class.
// Supports DVZip stored and independently zlib-wrapped records with odc CPIO.
// gzip, raw CPIO and newc are rejected; no whole-buffer fallback exists.
class Decoder {
 public:
  Decoder() = default;
  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;
  bool reset(const Manifest& manifest, Sink sink);
  Status feed(const uint8_t* input, size_t length, size_t& consumed);
  // Call only after the HTTP framing layer has proved complete body receipt.
  // Complete authorizes storage final validation, never sender success itself.
  Status finish();
  const char* error() const { return error_; }
  uint32_t payloadBytes() const { return payloadBytes_; }
  uint32_t encodedBytes() const { return encodedBytes_; }
  uint32_t archiveBytes() const { return archiveBytes_; }

 private:
  enum class CpioState { Header, Name, Body, Trailer };
  Manifest manifest_{};
  Sink sink_{};
  const char* error_ = "not initialized";
  bool initialized_ = false;
  bool done_ = false;
  bool dvzipEnd_ = false;
  bool fileSeen_ = false;
  bool rootSeen_ = false;
  bool sidecarSeen_ = false;
  bool currentFile_ = false;
  bool compressed_ = false;
  bool inflateDone_ = false;
  uint8_t recordHeader_[4]{};
  uint8_t recordHeaderBytes_ = 0;
  uint32_t recordRemaining_ = 0;
  uint32_t recordExpanded_ = 0;
  uint32_t records_ = 0;
  uint32_t encodedBytes_ = 0;
  uint32_t archiveBytes_ = 0;
  uint32_t payloadBytes_ = 0;
  uint32_t metadataBytes_ = 0;
  uint32_t entries_ = 0;
  CpioState cpioState_ = CpioState::Header;
  uint8_t cpioHeader_[76]{};
  uint8_t headerBytes_ = 0;
  char memberName_[kMaxName + 5]{};
  uint32_t nameBytes_ = 0;
  uint32_t nameSize_ = 0;
  uint32_t memberSize_ = 0;
  uint32_t memberRemaining_ = 0;
  uint32_t memberMode_ = 0;
  uint32_t memberLinks_ = 0;
  uint32_t trailerPadding_ = 0;
  tinfl_decompressor inflater_{};
  uint8_t window_[TINFL_LZ_DICT_SIZE]{};
  size_t windowPosition_ = 0;
  size_t pendingStart_ = 0;
  size_t pendingBytes_ = 0;

  Status fail(const char* reason);
  bool cancelled() const;
  bool parseHeader();
  bool admitName();
  void endMember();
  Status cpio(const uint8_t* bytes, size_t length, size_t& consumed);
  Status drain();
};

bool safeName(const char* name, size_t length);

}  // namespace airdrop::stream
