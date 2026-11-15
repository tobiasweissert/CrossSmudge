#pragma once
#include <cstdint>
#include <string>
#include <LuaHttp.h>
// NVS opt-out is outside Lua's filesystem. HTTPS obtains UTC from NTP automatically.
namespace HalLuaHttp {
bool networkEnabled();
bool setNetworkEnabled(bool enabled);
struct TimeProposal { int64_t epoch; int64_t monotonicUs; };
TimeProposal proposeTime();
bool trustTime(TimeProposal proposal);
bool timeTrusted();
bool wifiConnected();
void disconnectWifi();
}
