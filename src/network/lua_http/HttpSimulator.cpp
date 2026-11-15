#ifdef SIMULATOR
#include "LuaHttp.h"
#include <string>
#include <HalLuaHttp.h>
// Simulator has no verified TLS HAL. It must fail closed, never report fixture data as network success.
namespace ink::http {
namespace {bool unavailable(void(*)(void*),void*){return false;}}
Executor platformExecutor(){return {nullptr,nullptr,unavailable};}
}
namespace HalLuaHttp {
static bool enabled = true;
bool networkEnabled(){return enabled;}
bool setNetworkEnabled(bool value){enabled=value;return true;}
TimeProposal proposeTime(){return {0,0};}
bool trustTime(TimeProposal){return false;}
bool timeTrusted(){return false;}
bool wifiConnected(){return false;}
void disconnectWifi(){}
}
#endif
