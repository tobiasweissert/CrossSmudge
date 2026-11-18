#pragma once
extern bool wifi;
namespace HalLuaHttp {
inline bool wifiConnected() { return wifi; }
}  // namespace HalLuaHttp
