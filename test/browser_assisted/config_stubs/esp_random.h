#pragma once
#include <cstddef>
inline void esp_fill_random(void* out,size_t n){auto* p=(unsigned char*)out;for(size_t i=0;i<n;++i)p[i]=i;}
