#include "HalBrowserBackend.h"
#if !defined(SIMULATOR) && defined(BOARD_HAS_PSRAM) && BOARD_HAS_PSRAM
#include <HalLuaHttp.h>
#include <esp_random.h>
namespace {
bool random(void*,unsigned char* out,size_t n){
 // The pinned esp_random.h requires enabled Wi-Fi/BT for fresh hardware entropy.
 if(!HalLuaHttp::wifiConnected())return false;
 esp_fill_random(out,n);return true;
}
}
#endif
namespace HalBrowserBackend {
ink::http::BackendAccess access(){
#if !defined(SIMULATOR) && defined(BOARD_HAS_PSRAM) && BOARD_HAS_PSRAM
 // Public fixed-origin transport: no SD/NVS read, import or embedded secret.
 return {nullptr,nullptr,nullptr,random};
#else
 return {};
#endif
}
}
