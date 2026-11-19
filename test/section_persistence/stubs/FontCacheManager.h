#pragma once
class FontCacheManager {
 public:
  void releaseSdFontCaches() {}
  bool scanning = false;
  bool isScanning() const { return scanning; }
};
