#pragma once

#include <cctype>
#include <string>
#include <string_view>

#include "HalStorage.h"

namespace FsHelpers {
inline bool checkFileExtension(std::string_view path, std::string_view extension) {
  if (path.size() < extension.size()) return false;
  path.remove_prefix(path.size() - extension.size());
  for (size_t i = 0; i < path.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(path[i])) != extension[i]) return false;
  return true;
}
inline bool hasEpubExtension(std::string_view path) { return checkFileExtension(path, ".epub"); }
inline bool hasXtcExtension(std::string_view path) {
  return checkFileExtension(path, ".xtc") || checkFileExtension(path, ".xtch");
}
inline bool hasTxtExtension(const std::string& path) { return checkFileExtension(path, ".txt"); }
inline bool directoryIterationFailed(const HalFile& directory) {
  return directory.allocationFailed() || directory.iterationFailed();
}
}  // namespace FsHelpers
