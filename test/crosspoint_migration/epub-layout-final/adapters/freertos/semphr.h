#pragma once
#include "FreeRTOS.h"
inline void* xSemaphoreCreateMutexStatic(StaticSemaphore_t* p) { return p; }
inline int xSemaphoreTake(void*, unsigned) { return 1; }
inline void xSemaphoreGive(void*) {}
