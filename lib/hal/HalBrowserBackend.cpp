#include "HalBrowserBackend.h"
#if !defined(SIMULATOR) && defined(BOARD_HAS_PSRAM) && BOARD_HAS_PSRAM
#include <HalStorage.h>
#include <HalLuaHttp.h>
#include <Logging.h>
#include <Memory.h>
#include <Preferences.h>
#include <esp_random.h>
#include <cstring>
namespace {
constexpr char Space[]="browser-backend";
bool read(void*,char (&token)[65]) {
 Preferences p;if(!p.begin(Space,true))return false;
 unsigned active=p.getUChar("active",255);
 size_t n=active<2?p.getBytes(active?"token1":"token0",token,64):0;p.end();token[64]=0;
 return n==64 && ink::http::backendToken(token);
}
const char* import(void*) {
 // Cold explicit action: 513-byte file scratch stays off the Lua task stack.
 auto bytes=makeUniqueNoThrow<char[]>(513);
 if(!bytes){LOG_ERR("B7","Configuration allocation failed");return "out_of_memory";}
 HalFile f=Storage.open("/.crosssmudge/private/browser-backend.json");
 if(!f){LOG_ERR("B7","Configuration open failed");return "config_unreadable";}
 size_t n=f.size();if(n>512){f.close();LOG_ERR("B7","Configuration size rejected");return "config_invalid";}
 int got=f.read(bytes.get(),n);f.close();char token[65]{};
 if(got!=int(n) || !ink::http::parseBackendConfig({bytes.get(),n},token)){ink::http::eraseBackendSecret(bytes.get(),513);LOG_ERR("B7","Configuration data rejected");return "config_invalid";}
 ink::http::eraseBackendSecret(bytes.get(),513);
 Preferences p;if(!p.begin(Space,false)){ink::http::eraseBackendSecret(token,65);LOG_ERR("B7","Configuration store unavailable");return "storage_failed";}
 unsigned old=p.getUChar("active",255);
 if(old>1){if(p.isKey("active")){p.end();ink::http::eraseBackendSecret(token,65);LOG_ERR("B7","Configuration selector unreadable");return "storage_failed";}old=0;}
 unsigned next=1-old;
 const char* key=next?"token1":"token0";char verify[65]{};
 bool ok=p.putBytes(key,token,64)==64 && p.getBytes(key,verify,64)==64 && !memcmp(token,verify,64);
 if(ok){ok=p.putUChar("active",next)==1 && p.getUChar("active",255)==next;if(!ok && (p.putUChar("active",old)!=1 || p.getUChar("active",255)!=old))LOG_ERR("B7","Configuration selector recovery failed");}
 p.end();ink::http::eraseBackendSecret(token,65);ink::http::eraseBackendSecret(verify,65);
 if(!ok){LOG_ERR("B7","Configuration verification failed");return "storage_failed";}
 return nullptr;
}
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
 return {nullptr,read,import,random};
#else
 return {};
#endif
}
}
