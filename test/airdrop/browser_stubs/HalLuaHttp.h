#pragma once
namespace HalLuaHttp {
inline bool connected = true;
inline int disconnects = 0;
inline bool wifiConnected() { return connected; }
inline void disconnectWifi() {
  ++disconnects;
  connected = false;
}
}  // namespace HalLuaHttp
