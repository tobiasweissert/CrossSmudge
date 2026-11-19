#include <HalDisplay.h>

#include <type_traits>

using HalMethod = bool (HalDisplay::*)(uint16_t, uint16_t, uint16_t, uint16_t, bool);
using SdkMethod = bool (freeink::FreeInkDisplay::*)(uint16_t, uint16_t, uint16_t, uint16_t, bool);
static_assert(std::is_same_v<EInkDisplay, freeink::FreeInkDisplay>);
static_assert(std::is_same_v<decltype(&HalDisplay::displayRetainedGrayscaleOverlay), HalMethod>);
static_assert(std::is_same_v<decltype(&freeink::FreeInkDisplay::displayRetainedGrayscaleOverlay), SdkMethod>);
bool rendererCall(HalDisplay& display, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool restore) {
  return display.displayRetainedGrayscaleOverlay(x, y, w, h, restore);
}
bool defaultRestore(HalDisplay& display) { return display.displayRetainedGrayscaleOverlay(1, 2, 3, 4); }
