#pragma once

#include <atomic>
#include <cstdint>

namespace airdrop::input {
// Only AirDrop consumes this clock. The main task tags a frame BEFORE polling
// GPIO; the render task brackets the complete physical paint. An input poll
// spanning either boundary cannot acquire the new view's identity at routing.
class Capture {
 public:
  struct Frame {
    uint32_t epoch = 0, serial = 0;
  };
  uint32_t begin() const { return epoch_.load(std::memory_order_acquire); }
  void end(uint32_t started) {
    frame_.epoch = started && !(started & 1) && started == begin() ? started : 0;
    ++frame_.serial;
  }
  Frame frame() const { return frame_; }  // Main task only, like the GPIO frame.
  void beginPaint() {
    const auto previous = begin();
    // Fail closed on exhaustion rather than reusing an old epoch.
    epoch_.store(previous < UINT32_MAX - 1 ? previous + 1 : UINT32_MAX, std::memory_order_release);
  }
  uint32_t completePaint() {
    const auto painting = begin();
    const auto completed = painting < UINT32_MAX ? painting + 1 : UINT32_MAX;
    epoch_.store(completed, std::memory_order_release);
    return completed & 1 ? 0 : completed;
  }

 private:
  std::atomic<uint32_t> epoch_{0};
  Frame frame_{};
};
inline Capture capture;

// Activity-owned, protected by RenderLock. No heap allocation or shared SDK
// behavior changes. Both inputs must first be observed neutral AFTER paint.
class ConsentGate {
 public:
  void painted(uint32_t epoch) {
    epoch_ = epoch;
    touchReady_ = confirmReady_ = touchStarted_ = false;
  }
  uint32_t epoch() const { return epoch_; }
  bool consume(Capture::Frame frame, bool contact, bool down, bool released, bool confirmHeld, bool confirmPressed,
               bool& touch, bool& confirm) {
    touch = confirm = false;
    if (!epoch_ || frame.epoch != epoch_ || frame.serial == lastSerial_) return false;
    lastSerial_ = frame.serial;
    if (!touchReady_) {
      if (!contact && !down && !released) touchReady_ = true;
    } else {
      if (down && contact) touchStarted_ = true;
      // The adapter may report only a completed tap (or both edges together).
      // A neutral frame after this paint proves it is not a pre-paint contact.
      touch = released || (touchStarted_ && (down || contact));
      if (released || !contact) touchStarted_ = false;
    }
    if (!confirmReady_) {
      if (!confirmHeld && !confirmPressed) confirmReady_ = true;
    } else if (confirmPressed) {
      confirm = true;
      confirmReady_ = false;
    }
    return true;
  }

 private:
  uint32_t epoch_ = 0, lastSerial_ = 0;
  bool touchReady_ = false, confirmReady_ = false, touchStarted_ = false;
};
}  // namespace airdrop::input
