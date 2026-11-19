#pragma once

#include <string>
#include <string_view>

#include "HalStorage.h"

namespace FsHelpers {
inline bool directoryCanBeEnumerated(const char* path) {
  auto file = Storage.open(path);
  return file && file.isDirectory();
}
inline bool resolveRootDirectoryIgnoreCase(const char* path, char* out, size_t n) {
  if (!Storage.exists(path)) return false;
  std::snprintf(out, n, "%s", path);
  return std::strlen(path) < n;
}
inline bool directoryIterationFailed(const HalFile& file) { return file.allocationFailed() || file.iterationFailed(); }
inline bool checkFileExtension(std::string_view path, const char* extension) { return path.ends_with(extension); }
inline bool hasEpubExtension(const std::string& path) { return checkFileExtension(path, ".epub"); }
inline bool hasXtcExtension(std::string_view path) {
  return checkFileExtension(path, ".xtc") || checkFileExtension(path, ".xtch");
}
}  // namespace FsHelpers
