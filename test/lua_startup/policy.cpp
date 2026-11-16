#include "AppNetworkPolicy.h"
#include <cassert>
int openResult=0,readResult=0,writeResult=0,commitResult=0,closes=0; uint8_t savedBlocked=0;
int main(){
 bool ok=false;
 openResult=ESP_ERR_NVS_NOT_FOUND;assert(ink::http::networkEnabled(ok) && ok && closes==0);
 openResult=42;assert(!ink::http::networkEnabled(ok) && !ok && closes==0);
 openResult=ESP_OK;readResult=ESP_ERR_NVS_NOT_FOUND;
 assert(ink::http::networkEnabled(ok) && ok && closes==1);
 readResult=42;assert(!ink::http::networkEnabled(ok) && !ok && closes==2);
 readResult=ESP_OK;savedBlocked=1;assert(!ink::http::networkEnabled(ok) && ok);
 savedBlocked=0;assert(ink::http::networkEnabled(ok) && ok);
 assert(ink::http::setNetworkEnabled(false));assert(!ink::http::networkEnabled(ok));
 assert(ink::http::setNetworkEnabled(true));assert(ink::http::networkEnabled(ok));
 writeResult=42;assert(!ink::http::setNetworkEnabled(false));writeResult=0;
 commitResult=42;assert(!ink::http::setNetworkEnabled(false));commitResult=0;
 openResult=42;assert(!ink::http::setNetworkEnabled(false));openResult=ESP_OK;
 savedBlocked=255;assert(!ink::http::networkEnabled(ok) && ok);
}
