#pragma once
#include <HardwareSerial.h>
#include <Print.h>
#include <WString.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
class __FlashStringHelper;
using std::max;
using std::min;
#define PROGMEM
#define IRAM_ATTR
#define PGM_P const char*
#define PSTR(s) (s)
inline constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, INPUT = 0, INPUT_PULLUP = 2;
extern unsigned long testNowMs;
inline unsigned long millis() { return testNowMs; }
inline unsigned long micros() { return testNowMs * 1000; }
inline void delay(unsigned long) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }
inline void yield() {}
struct TestESP {
  unsigned getFreeHeap() { return 1000000; }
  unsigned getMaxAllocHeap() { return 1000000; }
};
inline TestESP ESP;
