#pragma once
#include <cstdint>
inline uint32_t uzlib_crc32(const void* p,unsigned int n,uint32_t crc){auto b=static_cast<const unsigned char*>(p);while(n--){crc^=*b++;for(int i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}return crc;}
