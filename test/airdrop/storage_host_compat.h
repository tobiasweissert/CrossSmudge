#pragma once
#include <cstdint>
class __FlashStringHelper;
inline uint32_t millis() { return 1; }
#ifndef F
#define F(str) (str)
#endif
