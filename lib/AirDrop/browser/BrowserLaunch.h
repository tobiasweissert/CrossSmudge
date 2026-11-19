#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>

namespace airdrop::browser {
enum class LaunchState : uint8_t { Empty, Pending, Offered, Acknowledged, Cancelled };
enum class LaunchError : uint8_t {
  None,
  InvalidUrl,
  InitFailed,
  IncompatibleBrowser,
  NetworkUnavailable,
  ScriptFailed,
  ExitedBeforeAck
};
// One immutable URL, shared by the coordinator and optional Lua child. Acknowledgment
// means Browser enqueued ordinary navigation, not that HTTPS or SD subsequently succeeded.
class LaunchRequest {
 public:
  bool prepare(std::string_view url);
  LaunchState state() const;
  LaunchError error() const { return problem.load(std::memory_order_acquire); }
  uint32_t beginAttempt();
  const char* take(uint32_t token);
  bool acknowledge(uint32_t token);
  bool failAttempt(uint32_t token, LaunchError error);
  void cancel();

 private:
  friend class LaunchHandle;
  void retain() { references.fetch_add(1, std::memory_order_relaxed); }
  void release() {
    if (references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
  }
  static constexpr uint32_t Taken = 0x10;
  static constexpr uint32_t MaxAttempt = 0x7fffff;
  std::atomic<uint32_t> references{1};
  std::atomic<uint32_t> word{static_cast<uint32_t>(LaunchState::Empty)};
  std::atomic<LaunchError> problem{LaunchError::None};
  char url[513]{};
};
// Intrusive ownership adds no control-block allocation. Coordinator allocates the
// bounded request once with makeUniqueNoThrow, checks it, then transfers into this handle.
class LaunchHandle {
 public:
  LaunchHandle() = default;
  explicit LaunchHandle(std::unique_ptr<LaunchRequest> request) : held(request.release()) {}
  LaunchHandle(const LaunchHandle& other) : held(other.held) {
    if (held) held->retain();
  }
  LaunchHandle(LaunchHandle&& other) noexcept : held(other.held) { other.held = nullptr; }
  ~LaunchHandle() {
    if (held) held->release();
  }
  LaunchHandle& operator=(const LaunchHandle& other) {
    if (this == &other) return *this;
    if (other.held) other.held->retain();
    if (held) held->release();
    held = other.held;
    return *this;
  }
  LaunchHandle& operator=(LaunchHandle&& other) noexcept {
    if (this == &other) return *this;
    if (held) held->release();
    held = other.held;
    other.held = nullptr;
    return *this;
  }
  explicit operator bool() const { return held != nullptr; }
  LaunchRequest* operator->() const { return held; }

 private:
  LaunchRequest* held = nullptr;
};
}  // namespace airdrop::browser
