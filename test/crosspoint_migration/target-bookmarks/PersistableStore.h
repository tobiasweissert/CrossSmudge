#pragma once
#include <ArduinoJson.h>

#include <map>
#include <string>
struct PersistableStoreBase {
  inline static std::map<std::string, std::string> files;
  inline static bool failWrite = false;
  static bool readDocFromFile(const char* path, JsonDocument& doc) {
    auto it = files.find(path);
    return it != files.end() && !deserializeJson(doc, it->second);
  }
  static bool writeDocToFile(const char* path, JsonDocument& doc) {
    if (failWrite) return false;
    files[path].clear();
    serializeJson(doc, files[path]);
    return true;
  }
};
