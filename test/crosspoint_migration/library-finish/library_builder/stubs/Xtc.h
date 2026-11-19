#pragma once

#include <string>

#include "HalStorage.h"

class Xtc {
  std::string path;

 public:
  Xtc(const std::string& path, const char*) : path(path) {}
  bool load() { return true; }
  template <class... T>
  bool generateThumbBmp(T&&...) {
    return true;
  }
  std::string getThumbBmpPath() { return ""; }
  std::string getTitle() const { return "XTC title"; }
  std::string getAuthor() const { return "XTC author"; }
  std::string getCachePath() const { return "/.crosspoint/cache_" + path.substr(1); }
  bool clearCache() const {
    const std::string cachePath = getCachePath();
    return !Storage.exists(cachePath.c_str()) || Storage.removeDir(cachePath.c_str());
  }
};
