#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// OTA is an owner-controlled Crosscub channel, not the legacy repository's
// latest arbitrary firmware. Use the owner's renamed canonical repository.
namespace crosscub_ota {
constexpr char releaseUrl[] = "https://api.github.com/repos/tobiasweissert/Crosscub/releases/latest";
constexpr char downloadRoot[] = "https://github.com/tobiasweissert/Crosscub/releases/download/";
constexpr char tagPrefix[] = "crosscub-v";

inline const char* version(const char* tag) {
  if (!tag || std::strncmp(tag, tagPrefix, sizeof(tagPrefix) - 1) != 0) return nullptr;
  const char* value = tag + sizeof(tagPrefix) - 1;
  if (*value < '0' || *value > '9') return nullptr;
  // No URL escapes, path separators, whitespace or query strings in tags.
  // Bound numeric runs before the inherited integer version comparator sees
  // them. Nine decimal digits cannot overflow a signed 32-bit segment.
  unsigned digits = 0;
  unsigned length = 0;
  for (const char* p = value; *p; ++p) {
    if (++length > 48) return nullptr;
    if (*p >= '0' && *p <= '9') {
      if (++digits > 9) return nullptr;
    } else {
      digits = 0;
    }
    if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '.' || *p == '-'))
      return nullptr;
  }
  return value;
}

inline bool acceptsDownload(const char* tag, const char* url, const char* sha256, size_t size) {
  if (!version(tag) || !url || !sha256 || std::strlen(sha256) != 64 || size < 65536) return false;
  for (size_t i = 0; i < 64; ++i) {
    const char c = sha256[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  size_t n = sizeof(downloadRoot) - 1;
  if (std::strncmp(url, downloadRoot, n) != 0) return false;
  url += n;
  n = std::strlen(tag);
  if (std::strncmp(url, tag, n) != 0 || url[n] != '/') return false;
  url += n + 1;
  constexpr char stem[] = "crosscub-sticky-v";
  n = sizeof(stem) - 1;
  if (std::strncmp(url, stem, n) != 0) return false;
  url += n;
  const char* v = version(tag);
  n = std::strlen(v);
  return std::strncmp(url, v, n) == 0 && std::strcmp(url + n, ".bin") == 0;
}

// ESP application descriptor is the first data segment, immediately following
// the 24-byte image header and 8-byte segment header. Do not accept a mere
// product string elsewhere in an upstream image as product identity.
inline bool acceptsImageDescriptor(const uint8_t* bytes, size_t size) {
  constexpr size_t descriptor = 32;
  constexpr size_t project = descriptor + 48;
  if (!bytes || size < project + 32) return false;
  if (bytes[0] != 0xe9 || bytes[descriptor] != 0x32 || bytes[descriptor + 1] != 0x54 || bytes[descriptor + 2] != 0xcd ||
      bytes[descriptor + 3] != 0xab)
    return false;
  constexpr char name[] = "Crosscub";
  return std::memcmp(bytes + project, name, sizeof(name)) == 0;
}
// Release convention: crosscub-v<V> and CROSSCUB_APP_VERSION == <V>.
// <V> is one to four decimal components (at most nine digits each), without
// the runtime display suffix "-sticky". Build provenance/display labels are
// separate from this stable release identity; the descriptor producer writes
// CROSSCUB_APP_VERSION rather than the display version.
inline bool acceptsImageVersion(const uint8_t* bytes, size_t size, const char* tag, const char* current,
                                int (*compare)(const char*, const char*)) {
  constexpr size_t offset = 32 + 16;  // esp_app_desc_t.version[32]
  constexpr size_t capacity = 32;
  if (!acceptsImageDescriptor(bytes, size) || size < offset + capacity || !compare) return false;
  const char* embedded = reinterpret_cast<const char*>(bytes + offset);
  const char* end = static_cast<const char*>(std::memchr(embedded, '\0', capacity));
  const char* selected = version(tag);
  if (!end || end == embedded || !selected) return false;
  unsigned components = 1;
  unsigned digits = 0;
  for (const char* p = embedded; p != end; ++p) {
    if (*p >= '0' && *p <= '9') {
      if (++digits > 9) return false;
    } else if (*p == '.' && digits && ++components <= 4) {
      digits = 0;
    } else {
      return false;
    }
  }
  return digits && std::strcmp(embedded, selected) == 0 && compare(embedded, current) > 0;
}
}  // namespace crosscub_ota
