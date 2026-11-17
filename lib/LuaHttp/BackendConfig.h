#pragma once
#include "HttpCore.h"
namespace ink::http {
inline void eraseBackendSecret(void* bytes,size_t n){auto* p=static_cast<volatile unsigned char*>(bytes);while(n--)*p++=0;}
// Strict, bounded provisioning parser. No general-purpose JSON allocation.
bool parseBackendConfig(std::string_view json, char (&token)[65]);
struct BackendAccess {
  void* context=nullptr;
  bool (*read)(void*,char (&)[65])=nullptr;
  const char* (*import)(void*)=nullptr; // nullptr means success; stable codes only.
  bool (*random)(void*,unsigned char*,size_t)=nullptr;
};
}
