#pragma once
#include <stdint.h>
// Sticky only. Once the private keyboard is opened, allocator/TLS copies may
// outlive Forget. Keep diagnostics private until the next boot consumes this latch.
namespace dictationPrivacy {
constexpr uint32_t marker = 0x44505431;
extern volatile uint32_t retained;
inline void arm() { retained = marker; }
inline bool active() { return retained == marker; }
}  // namespace dictationPrivacy
