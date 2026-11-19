#pragma once
#include <ArduinoJson.h>

#include <cstring>

#include "HttpCore.h"
namespace ink::http {
inline bool readNetworkDeclaration(JsonVariantConst network, Domains& domains) {
  domains = {};
  if (network.isNull()) return true;
  if (!network.is<JsonObjectConst>() || network.size() != 1 || !network["domains"].is<JsonArrayConst>()) return false;
  for (JsonVariantConst domain : network["domains"].as<JsonArrayConst>()) {
    if (!domain.is<const char*>()) {
      domains = {};
      return false;
    }
    JsonString text = domain.as<JsonString>();
    if (!domains.add(std::string_view(text.c_str(), text.size()))) {
      domains = {};
      return false;
    }
  }
  // Normalize declaration order before comparing a persisted grant.
  for (size_t i = 0; i < domains.count; ++i) {
    for (size_t j = i + 1; j < domains.count; ++j) {
      if (strcmp(domains.names[i], domains.names[j]) > 0) {
        char temp[128];
        memcpy(temp, domains.names[i], sizeof(temp));
        memcpy(domains.names[i], domains.names[j], sizeof(temp));
        memcpy(domains.names[j], temp, sizeof(temp));
      }
    }
  }
  return true;
}
}  // namespace ink::http
