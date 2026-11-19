#pragma once
#include <cstdint>
// Board hardware profile double, not a display API double.
namespace BoardConfig {
enum class DisplayController { SSD2677, UC8179 };
inline constexpr uint32_t MAX_FRAMEBUFFER_BYTES = 48000;
struct Profile {
  DisplayController displayController = DisplayController::SSD2677;
};
inline constexpr Profile ACTIVE{};
}  // namespace BoardConfig
