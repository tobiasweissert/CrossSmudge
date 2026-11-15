#pragma once
#include "ChatCore.h"
namespace stickychat {
struct Microphone {
  virtual ~Microphone() = default;
  virtual bool begin() = 0;
  virtual size_t read(uint8_t* out, size_t max) = 0;
  virtual void end() = 0;
};
class Capture {
 public:
  explicit Capture(Microphone& mic) : mic(mic) {}
  ~Capture() { cancel(); }
  bool start(uint8_t* storage, size_t capacity, uint32_t now);
  void tick(uint32_t now);
  void stop();
  void cancel();
  bool recording() const { return live; }
  size_t size() const { return valid ? used + 44 : 0; }

 private:
  Microphone& mic;
  uint8_t* bytes = nullptr;
  size_t used = 0;
  uint32_t started = 0;
  bool live = false, valid = false;
};
}  // namespace stickychat
