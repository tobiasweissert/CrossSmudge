#include "admission.h"
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
 // Execute the exact production startup admission assignment against changing
 // validated declaration sets; no per-domain grant or old grant hash exists.
 savedBlocked=0;
 Admission app;
 app.launch();assert(!app.http_.allowed); // offline manifest
 assert(app.http_.domains.add("example.com"));
 openResult=ESP_ERR_NVS_NOT_FOUND;app.launch();assert(app.http_.allowed);
 openResult=ESP_OK;readResult=ESP_ERR_NVS_NOT_FOUND;app.launch();assert(app.http_.allowed);
 readResult=ESP_OK;
 assert(ink::http::setNetworkEnabled(false));
 app.launch();assert(!app.http_.allowed);
 app.http_.domains={};assert(app.http_.domains.add("changed.example.com"));
 app.launch();assert(!app.http_.allowed); // a manifest update must not undo opt-out
 assert(ink::http::setNetworkEnabled(true));app.launch();assert(app.http_.allowed);
 assert(app.http_.domains.add("second.example.com"));app.launch();assert(app.http_.allowed);
 app.http_.domains={};app.launch();assert(!app.http_.allowed);
}
