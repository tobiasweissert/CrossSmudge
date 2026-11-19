#pragma once
using gpio_num_t = int;
inline int gpio_hold_dis(int) { return 0; }
inline int gpio_hold_en(int) { return 0; }
inline int gpio_set_level(int, int) { return 0; }
