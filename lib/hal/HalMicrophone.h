#pragma once
#if defined(FREEINK_DEVICE_STICKY)
#include <ESP_I2S.h>
#include <cstddef>
#include <cstdint>
// Single owner; the owner drains capture before destruction or sleep.
class HalMicrophone {
 public:
  ~HalMicrophone() { end(); }
  bool begin();
  size_t read(uint8_t* out, size_t capacity);
  void end();
 private:
  I2SClass i2s;
  bool live = false;
};
#endif
