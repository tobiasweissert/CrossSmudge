#pragma once
#include <chrono>

#include "../../../chapter_html_slim_parser/stubs/Arduino.h"
inline void yield() {}
inline uint32_t micros() {
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
