#pragma once
#include <chrono>
#include <cstdint>
struct EspHostStub {
  uint32_t getFreeHeap() const { return UINT32_MAX; }
  uint32_t getMaxAllocHeap() const { return UINT32_MAX; }
};
inline EspHostStub ESP;
inline uint32_t micros() {
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
