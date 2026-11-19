#pragma once
#include <chrono>
#include <thread>
inline uint32_t millis() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
inline void delay(unsigned n) { std::this_thread::sleep_for(std::chrono::milliseconds(n)); }
inline uint32_t micros() {
  using namespace std::chrono;
  return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
