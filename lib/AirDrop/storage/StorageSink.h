#pragma once

#include <cstddef>
#include <cstdint>

namespace airdrop::storage {
constexpr uint32_t MaxPayloadBytes = 30000000;
constexpr size_t ChunkBytes = 1024;
constexpr size_t PathBytes = 192;

enum class State : uint8_t { Idle, Receiving, CompleteStaged, PublishAttempted, PublishedVerified, Uncertain, Failed, Cancelled, RetainedPartial };
enum class PathState : uint8_t { Missing, File, Directory, Unreadable };

// All operations are checked. stat(Missing) must come from a healthy parent
// directory scan, never open/exists(false). renameExclusive MUST preserve an
// existing destination and may mutate either path even when it returns false.
// Reader and stage-writer ownership are distinct; each has one open handle.
class Files {
 public:
  virtual ~Files() = default;
  virtual bool ready(uint64_t& freeBytes) = 0;
  virtual PathState stat(const char* path, uint32_t& size) = 0;
  virtual bool mkdirExclusive(const char* path) = 0;
  virtual bool createStageExclusive(const char* path) = 0;
  virtual size_t writeStage(const uint8_t* bytes, size_t length) = 0;
  virtual bool syncStage() = 0;
  virtual bool closeStage() = 0;
  virtual bool writeRecordExclusive(const char* path, const uint8_t* bytes, size_t length) = 0;
  virtual bool openRead(const char* path, uint32_t& size) = 0;
  virtual int read(uint8_t* bytes, size_t length) = 0;
  virtual bool seek(uint32_t offset) = 0;
  virtual bool closeRead() = 0;
  virtual bool renameExclusive(const char* oldPath, const char* newPath) = 0;
  virtual bool hashStart() = 0;
  virtual bool hashUpdate(const uint8_t* bytes, size_t length) = 0;
  virtual bool hashFinish(uint8_t digest[32]) = 0;
  virtual bool cancelled() const = 0;
};

// Fixed session state and bounded read/journal buffers. No internal allocation.
// Parent owns its lifetime until listener and SD callbacks have cooperatively
// joined. All writes are exclusively under an owned /Inbox/.airdrop/<nonce>.
class StorageSink {
 public:
  explicit StorageSink(Files& files) : files_(files) {}
  StorageSink(const StorageSink&) = delete;
  StorageSink& operator=(const StorageSink&) = delete;
  bool begin(const char* safeName, uint32_t size, const uint8_t nonce[16]);
  bool append(const uint8_t* bytes, size_t length);
  // Requires complete framing/archive/accepted-manifest validation upstream.
  bool finish();
  void cancel();
  // Read-only recovery, including on a fresh mount. Never repairs/removes or
  // republishes either possible alias. token is exactly 32 lowercase hex bytes.
  State inspect(const char* token);
  // Validate durable terminal receipts without rehashing historical books. A
  // PublishedVerified return here describes the checked receipt, not current
  // content (which may have been deliberately moved/deleted by its owner).
  State inspectJournal(const char* token);
  State state() const { return state_; }
  const char* error() const { return error_; }
  const char* savedPath() const { return state_ == State::PublishedVerified ? finalPath_ : ""; }
  const char* recoveryPath() const { return sessionPath_; }
  uint32_t payloadBytes() const { return written_; }
  uint32_t committedBytes() const { return state_ == State::PublishedVerified ? expected_ : 0; }

 private:
  Files& files_;
  State state_ = State::Idle;
  const char* error_ = nullptr;
  bool stageOpen_ = false;
  bool publicationAttempted_ = false;
  uint32_t expected_ = 0;
  uint32_t written_ = 0;
  char token_[33]{};
  char name_[121]{};
  char sessionPath_[64]{};
  char stagePath_[80]{};
  char finalPath_[PathBytes]{};
  char recordPath_[96]{};
  uint8_t digest_[32]{};
  uint8_t journal_[512]{};
  uint8_t buffer_[ChunkBytes]{};

  bool fail(const char* reason, bool uncertain = false);
  bool paths(const char* token);
  bool ensureDirectory(const char* path);
  bool record(uint8_t phase);
  bool loadRecord(uint8_t phase);
  void serialize(uint8_t phase);
  bool verify(const char* path);
  State inspectImpl(const char* token, bool verifyPayload);
};

uint32_t journalCrc32(const uint8_t* bytes, size_t length);
}  // namespace airdrop::storage
