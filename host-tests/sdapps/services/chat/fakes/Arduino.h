#pragma once
#include <cstdint>

#include "PlatformFakes.h"
inline unsigned millis() { return fake::clock++; }
inline void delay(unsigned n) { fake::clock += n; }
inline constexpr int OUTPUT = 1, HIGH = 1, LOW = 0;
inline int micPower = 0;
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int state) {
  if (pin == 38) micPower = state;
}
