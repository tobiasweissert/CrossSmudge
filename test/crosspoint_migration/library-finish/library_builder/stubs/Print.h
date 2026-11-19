#pragma once
#include <cstdint>
#include <cstddef>
class Print { public: virtual ~Print()=default; virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t* b,size_t n){size_t i=0;for(;i<n&&write(b[i]);++i){}return i;} };
