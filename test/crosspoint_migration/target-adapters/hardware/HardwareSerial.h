#pragma once
#include <cstdint>
class HardwareSerial {
 public:
  operator bool() const { return false; }
  template <class... T>
  void printf(const char*, T...) {}
  void flush() {}
  void begin(unsigned long) {}
  void setTxTimeoutMs(unsigned) {}
};
inline HardwareSerial Serial, Serial0;
