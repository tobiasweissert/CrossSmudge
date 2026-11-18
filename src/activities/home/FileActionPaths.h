#pragma once

#include <FsHelpers.h>
#include <string>

namespace FileActionPaths {
// Session-wide fail-closed guard after an unacknowledged storage operation.
// Recovery is deliberately read-only/off-device; reboot is not proof of repair.
inline bool mutationBlocked = false;
// Manual file operations never reach hidden application state, even when the
// browser's Show hidden files preference is enabled. Do not canonicalize names.
inline bool isUserPath(const std::string& path) {
  if (path.empty() || path.front() != '/' || path.find('\0') != std::string::npos) return false;
  if (path == "/") return true;
  size_t start = 1;
  while (start < path.size()) {
    const auto end = path.find('/', start);
    const auto part = path.substr(start, end == std::string::npos ? end : end - start);
    if (part.empty() || part.front() == '.' || !FsHelpers::isSafePathComponent(part)) return false;
    if (end == std::string::npos) return true;
    start = end + 1;
  }
  return false;
}
}  // namespace FileActionPaths
