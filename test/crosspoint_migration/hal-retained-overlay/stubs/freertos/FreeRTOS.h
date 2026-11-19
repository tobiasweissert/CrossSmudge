#pragma once
using BaseType_t = int;
using TickType_t = unsigned long;
inline constexpr BaseType_t pdTRUE = 1;
inline constexpr TickType_t portMAX_DELAY = ~TickType_t{0};
