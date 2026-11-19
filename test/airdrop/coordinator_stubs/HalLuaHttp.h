#pragma once
namespace HalLuaHttp {
inline bool connected = false, enabled = true;
inline int disconnects = 0, enabledReads = 0;
inline bool wifiConnected() { return connected; }
inline bool networkEnabled() {
  ++enabledReads;
  return enabled;
}
inline void disconnectWifi() {
  ++disconnects;
  connected = false;
}
}  // namespace HalLuaHttp
