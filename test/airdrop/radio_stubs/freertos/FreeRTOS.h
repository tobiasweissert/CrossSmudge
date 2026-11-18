#pragma once
// Explicit host semantic seam for the ESP32 branch: this is not the FreeRTOS
// port, interrupt masking, fairness, timing, or an RF callback implementation.
#include <atomic>
#include <cassert>
#include <mutex>
struct portMUX_TYPE {
  std::mutex mutex;
};
#define portMUX_INITIALIZER_UNLOCKED \
  {                                  \
  }
inline std::atomic<unsigned> radioCriticalCalls{0};
inline thread_local unsigned radioCriticalDepth = 0;
inline void radioEnterCritical(portMUX_TYPE* mux) {
  assert(radioCriticalDepth == 0);
  mux->mutex.lock();
  ++radioCriticalDepth;
  ++radioCriticalCalls;
}
inline void radioExitCritical(portMUX_TYPE* mux) {
  assert(radioCriticalDepth == 1);
  --radioCriticalDepth;
  mux->mutex.unlock();
}
#define portENTER_CRITICAL(mux) radioEnterCritical(mux)
#define portEXIT_CRITICAL(mux) radioExitCritical(mux)
