#pragma once
#include <string>
#include <cstring>
using nvs_handle_t=int;
inline constexpr int ESP_OK=0, ESP_ERR_NVS_NOT_FOUND=1,NVS_READONLY=0,NVS_READWRITE=1;
namespace fake { inline std::string nvsKey; inline bool nvsFail=false; }
inline int nvs_open(const char*,int,nvs_handle_t* h) { *h=1; return fake::nvsFail ? 2 : 0; }
inline int nvs_get_str(int,const char*,char* out,size_t* size) {
 if(fake::nvsKey.empty()) return ESP_ERR_NVS_NOT_FOUND;
 if(out) { if(*size<fake::nvsKey.size()+1) return 2; std::memcpy(out,fake::nvsKey.c_str(),fake::nvsKey.size()+1); }
 *size=fake::nvsKey.size()+1; return 0;
}
inline int nvs_set_str(int,const char*,const char* value) { fake::nvsKey=value; return 0; }
inline int nvs_erase_key(int,const char*) { fake::nvsKey.clear(); return 0; }
inline int nvs_commit(int) { return 0; }
inline void nvs_close(int) {}
