#include <EInkDisplay.h>

#include <type_traits>
using SdkMethod = bool (freeink::FreeInkDisplay::*)(uint16_t, uint16_t, uint16_t, uint16_t, bool);
static_assert(std::is_same_v<EInkDisplay, freeink::FreeInkDisplay>);
static_assert(std::is_same_v<decltype(&freeink::FreeInkDisplay::displayRetainedGrayscaleOverlay), SdkMethod>);
bool sdkDefaultRestore(EInkDisplay& display) { return display.displayRetainedGrayscaleOverlay(1, 2, 3, 4); }
