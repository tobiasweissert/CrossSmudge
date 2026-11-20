#pragma once
#include <cstdint>

namespace FindMySound {
enum class Source : uint8_t { LocalManual, NativeOwner, NonOwner, Unknown };
enum class Result : uint8_t { Disabled, UnsupportedSource, Unauthorized, InvalidDuration, Busy, DriverFailure, Started };
// Trusted in-process boundary, NOT a network authorization protocol.
// Context outlives Controller. Callbacks are synchronous, non-reentrant, bounded,
// and must not throw. start must enforce its own <=duration hardware timeout;
// returning true only confirms driver acceptance, not measured audible output.
// stop returns true only when the driver can establish output is off.
// localAuthorized reads trusted foreground/physical-test policy, never wire input.
struct Driver {
  void* context = nullptr;
  bool (*start)(void*, uint32_t) = nullptr;
  bool (*stop)(void*) = nullptr;
  bool (*localAuthorized)(void*) = nullptr;
};
class Controller {
 public:
  explicit Controller(Driver driver) : driver_(driver) {}
  ~Controller() { stop(); }
  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;
  bool available() const { return driver_.start && driver_.stop && driver_.localAuthorized; }
  bool enabled() const { return enabled_; }
  bool active() const { return active_; }
  bool faulted() const { return faulted_; }
  bool setEnabled(bool value) {
    if (!value) {
      enabled_ = false;
      return stop();
    }
    if (faulted_) return false;
    enabled_ = available();
    return enabled_;
  }
  bool stop() {
    if (!active_) return true;
    if (!driver_.stop(driver_.context)) {
      faulted_ = true;
      enabled_ = false;
      return false;
    }
    active_ = false;
    faulted_ = false;
    return true;
  }
  void tick(uint32_t now) {
    if (active_ && (faulted_ || !driver_.localAuthorized(driver_.context) ||
                    static_cast<uint32_t>(now - started_) >= duration_)) {
      stop();
    }
  }
  Result start(Source source, uint32_t duration, uint32_t now) {
    if (!enabled_) return Result::Disabled;
    if (source != Source::LocalManual) return Result::UnsupportedSource;
    if (!driver_.localAuthorized(driver_.context)) return Result::Unauthorized;
    if (duration == 0 || duration > 5000) return Result::InvalidDuration;
    if (active_) return Result::Busy;
    if (!driver_.start(driver_.context, duration)) {
      // A failed start can have partially energized hardware: stop defensively.
      enabled_ = false;
      active_ = true;
      stop();
      return Result::DriverFailure;
    }
    started_ = now;
    duration_ = duration;
    active_ = true;
    return Result::Started;
  }
 private:
  bool enabled_ = false;
  bool active_ = false;
  bool faulted_ = false;
  uint32_t started_ = 0, duration_ = 0;
  Driver driver_;
};
}  // namespace FindMySound
