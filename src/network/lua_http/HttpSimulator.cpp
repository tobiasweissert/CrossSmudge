#ifdef SIMULATOR
#include <HalLuaHttp.h>

#include <string>

#include "LuaHttp.h"
// Simulator has no verified TLS HAL. It must fail closed, never report fixture data as network success.
namespace ink::http {
namespace {
bool unavailable(void (*)(void*), void*) { return false; }
}  // namespace
Executor platformExecutor() { return {nullptr, nullptr, unavailable}; }
}  // namespace ink::http
namespace HalLuaHttp {
static bool enabled = true;
bool networkEnabled() { return enabled; }
bool setNetworkEnabled(bool value) {
  enabled = value;
  return true;
}
TimeProposal proposeTime() { return {0, 0}; }
bool trustTime(TimeProposal) { return false; }
bool timeTrusted() { return false; }
bool wifiConnected() { return false; }
void disconnectWifi() {}
}  // namespace HalLuaHttp
#endif
