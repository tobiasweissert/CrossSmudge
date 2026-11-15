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
bool approved(const std::string&,const ink::http::Domains&){return false;}
bool approve(const std::string&,const ink::http::Domains&){return false;}
bool revokeAll(){return false;}
TimeProposal proposeTime(){return {0,0};}
bool trustTime(TimeProposal){return false;}
bool timeTrusted(){return false;}
bool wifiConnected(){return false;}
void disconnectWifi(){}
}
#endif
