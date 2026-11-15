#pragma once
// Type declarations/macros for the library's non-Arduino print-only paths.
#include <cstdint>
class __FlashStringHelper;
// Only used by the real LFN short-name collision resolver; fixture uses 8.3 names.
inline uint32_t millis() { return 1; }
#ifndef F
#define F(str) (str)
#endif
