#pragma once
#include <cstdlib>
inline void* fiFontMalloc(size_t n) { return malloc(n); }
inline void* fiFontRealloc(void* p, size_t n) { return realloc(p, n); }
inline void fiFontFree(void* p) { free(p); }
