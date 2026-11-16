#pragma once
#include <cstdint>
#include <string>
#include <LuaHttp.h>
// NVS grants are outside Lua's SD filesystem. Trust in UTC is explicit, per boot.
namespace HalLuaHttp {
bool approved(const std::string& appDir, const ink::http::Domains& domains);
bool approve(const std::string& appDir, const ink::http::Domains& domains);
bool revokeAll();
struct TimeProposal { int64_t epoch; int64_t monotonicUs; };
TimeProposal proposeTime();
bool trustTime(TimeProposal proposal);
bool timeTrusted();
bool wifiConnected();
void disconnectWifi();
}
