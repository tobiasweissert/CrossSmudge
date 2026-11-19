#pragma once
#include <ArduinoJson.h>
#include <BlePageTurner.h>
namespace bleturner {
inline void writeJson(const Config& c, JsonDocument& d) { d["bleEnabled"] = c.enabled; }
inline bool readJson(Config& c, JsonVariantConst d) {
  c.enabled = d["bleEnabled"] | false;
  return true;
}
}  // namespace bleturner
