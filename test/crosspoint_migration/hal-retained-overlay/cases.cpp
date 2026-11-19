#include <HalDisplay.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "HalSpiBus.h"

namespace {
int depth = 0;
int expectedDepth = 1;
int calls = 0;
bool sdkResult = false;
bool throwAtSdk = false;
std::array<uint16_t, 4> observed{};
bool observedRestore = false;
std::vector<std::string> events;
int mutexToken;
void require(bool condition) {
  if (!condition) throw std::runtime_error("retained overlay assertion failed");
}
void reset() {
  require(depth == 0);
  calls = 0;
  expectedDepth = 1;
  throwAtSdk = false;
  events.clear();
}
void check(std::array<uint16_t, 4> args, bool restore, bool result) {
  reset();
  sdkResult = result;
  const bool actual = display.displayRetainedGrayscaleOverlay(args[0], args[1], args[2], args[3], restore);
  require(actual == result && calls == 1 && observed == args && observedRestore == restore && depth == 0);
  require(events == std::vector<std::string>({"take", "delegate", "give"}));
}
}  // namespace

// RTOS hardware doubles; the real HalSpiBus.cpp owns Lock construction/destruction.
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return &mutexToken; }
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, TickType_t wait) {
  require(mutex == &mutexToken && wait == portMAX_DELAY);
  ++depth;
  events.emplace_back("take");
  return pdTRUE;
}
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t mutex) {
  require(mutex == &mutexToken && depth > 0);
  events.emplace_back("give");
  --depth;
  return pdTRUE;
}

// Link doubles on the REAL pinned SDK class declaration. No fake EInkDisplay header.
// Panel implementation is intentionally not linked: this suite tests HAL forwarding only.
namespace freeink {
FreeInkDisplay::FreeInkDisplay(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {}
EpdBus::~EpdBus() = default;
bool FreeInkDisplay::displayRetainedGrayscaleOverlay(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool restore) {
  require(depth == expectedDepth);
  events.emplace_back("delegate");
  ++calls;
  observed = {x, y, w, h};
  observedRestore = restore;
  if (throwAtSdk) throw std::runtime_error("synthetic SDK throw");
  return sdkResult;
}
}  // namespace freeink

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string test = argv[1];
  try {
    if (test == "arguments_success")
      check({17, 29, 311, 157}, false, true);
    else if (test == "restore_success")
      check({7, 13, 119, 43}, true, true);
    else if (test == "result_false")
      check({19, 23, 47, 59}, false, false);
    else if (test == "restore_false")
      check({31, 37, 61, 67}, true, false);
    else if (test == "zero_geometry_delegated")
      check({0, 0, 0, 0}, false, false);
    else if (test == "uint16_boundary_delegated")
      check({65535, 65534, 65533, 65532}, true, false);
    else if (test == "default_restore_false") {
      reset();
      sdkResult = true;
      require(display.displayRetainedGrayscaleOverlay(1, 2, 3, 4));
      require(calls == 1 && observed == std::array<uint16_t, 4>{1, 2, 3, 4} && !observedRestore && depth == 0);
      require(events == std::vector<std::string>({"take", "delegate", "give"}));
    } else if (test == "recursive_lock_lifetime") {
      reset();
      sdkResult = true;
      expectedDepth = 2;
      {
        HalSpiBus::Lock outer;
        require(display.displayRetainedGrayscaleOverlay(1, 2, 3, 4, true));
        require(depth == 1 && calls == 1);
      }
      require(depth == 0);
      require(events == std::vector<std::string>({"take", "take", "delegate", "give", "give"}));
    } else if (test == "exception_lock_lifetime") {
      reset();
      throwAtSdk = true;
      bool caught = false;
      try {
        display.displayRetainedGrayscaleOverlay(1, 2, 3, 4);
      } catch (const std::runtime_error& e) {
        caught = std::string(e.what()) == "synthetic SDK throw";
      }
      require(caught && calls == 1 && depth == 0);
      require(events == std::vector<std::string>({"take", "delegate", "give"}));
    } else
      return 2;
    std::cout << "PASS " << test << '\n';
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL " << test << ": " << e.what() << '\n';
    return 1;
  }
}
