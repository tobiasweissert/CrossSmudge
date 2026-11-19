#pragma once
#include <cstddef>
#include <cstdint>
inline constexpr int HIGH = 1;
inline constexpr int LOW = 0;
inline int digitalRead(int) { return LOW; }
inline void delay(unsigned long) {}
