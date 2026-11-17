#pragma once
#include "HttpCore.h"
namespace ink::http {
inline void eraseBackendSecret(void* bytes,size_t n){auto* p=static_cast<volatile unsigned char*>(bytes);while(n--)*p++=0;}
// Legacy parser retained for isolated regression coverage; never used by HAL/Lua.
bool parseBackendConfig(std::string_view json, char (&token)[65]);
struct BackendAccess {
  void* context=nullptr;
  bool (*read)(void*,char (&)[65])=nullptr; // Retired, never called by public lane.
  const char* (*import)(void*)=nullptr; // Retired, never called by public lane.
  bool (*random)(void*,unsigned char*,size_t)=nullptr;
};
}
