#include "ProviderKey.h"
#include "nvs.h"
#include <cassert>
#include <cstdio>
using namespace stickychat;
int main() {
 std::string key="stale";
 assert(!loadProviderKey(key) && key.empty());
 assert(saveProviderKey("sk-device-canary"));
 assert(loadProviderKey(key) && key=="sk-device-canary");
 assert(!saveProviderKey("bad\r\nvalue"));
 assert(loadProviderKey(key) && key=="sk-device-canary");
 assert(saveProviderKey("sk-replaced"));
 assert(loadProviderKey(key) && key=="sk-replaced");
 fake::nvsFail=true; assert(!removeProviderKey()); assert(!loadProviderKey(key) && key.empty());
 fake::nvsFail=false; assert(removeProviderKey()); assert(!loadProviderKey(key));
 fake::nvsKey=std::string(513,'x'); assert(!loadProviderKey(key) && key.empty());
 puts("NVS bounded enter/replace/remove/failure (fake storage): PASS");
}
