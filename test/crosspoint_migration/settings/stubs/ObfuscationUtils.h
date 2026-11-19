#pragma once
#include <Arduino.h>

#include <cstddef>
#include <string>
namespace obfuscation {
inline std::string deobfuscateFromBase64(const char* p, size_t limit, bool* ok, bool* tooLong) {
  std::string s(p);
  *tooLong = s.size() > limit;
  *ok = !*tooLong;
  return *ok ? s : "";
}
}  // namespace obfuscation
