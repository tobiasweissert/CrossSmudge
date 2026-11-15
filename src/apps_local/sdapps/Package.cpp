#include "Package.h"
#include <cstring>
#include <string_view>

namespace sdapps {
namespace {
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
bool id(std::string_view text) {
  if (text.empty() || text.size() >= sizeof(Package::id)) return false;
  for (char c : text) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}
}

bool validPackageId(const char* name) {
  if (!name) return false;
  const size_t length = strnlen(name, sizeof(Package::id));
  return id(std::string_view(name, length));
}

bool parsePackage(const char* bytes, size_t length, const char* directory, Package& out) {
  out = {};
  if (!bytes || !validPackageId(directory) || !length || length > MAX_MANIFEST_BYTES) return false;
  std::string_view text(bytes, length);
  constexpr std::string_view MAGIC = "CROSSPLAY-APP 1\n";
  if (!text.starts_with(MAGIC) || text.back() != '\n') return false;
  text.remove_prefix(MAGIC.size());
  Package parsed{};
  unsigned seen = 0;
  while (!text.empty()) {
    const size_t end = text.find('\n');
    if (end == std::string_view::npos) return false;
    const auto line = text.substr(0, end);
    text.remove_prefix(end + 1);
    const size_t equal = line.find('=');
    if (equal == std::string_view::npos) return false;
    const auto key = line.substr(0, equal);
    const auto value = line.substr(equal + 1);
    unsigned field;
    if (key == "id") {
      field = 1;
      if (!id(value) || value != directory) return false;
      memcpy(parsed.id, value.data(), value.size());
    } else if (key == "title") {
      field = 2;
      if (value.empty() || value.size() >= sizeof(parsed.title)) return false;
      for (unsigned char c : value) if (c < 32 || c > 126) return false;
      memcpy(parsed.title, value.data(), value.size());
    } else if (key == "target") {
      field = 4;
      if (value != "esp32s3") return false;
    } else if (key == "abi") {
      field = 8;
      if (value != "1" && value != "2") return false;
      parsed.abi = value == "2" ? 2 : 1;
    } else if (key == "image") {
      field = 16;
      if (value != "app.elf") return false;
    } else if (key == "sha256") {
      field = 32;
      if (value.size() != 64) return false;
      for (size_t i = 0; i < 32; ++i) {
        const int hi = hex(value[2 * i]);
        const int lo = hex(value[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        parsed.digest[i] = static_cast<uint8_t>((hi << 4) | lo);
      }
    } else {
      return false;
    }
    if (seen & field) return false;
    seen |= field;
  }
  if (seen != 63) return false;
  out = parsed;
  return true;
}
}  // namespace sdapps
