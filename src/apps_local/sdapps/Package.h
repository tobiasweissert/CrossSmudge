#pragma once
#include <cstddef>
#include <cstdint>

namespace sdapps {
inline constexpr size_t MAX_MANIFEST_BYTES = 320;
inline constexpr size_t MAX_IMAGE_BYTES = 1024 * 1024;
inline constexpr size_t MAX_PACKAGES_PER_FOLDER = 16;
struct Package {
  char id[32];
  char title[48];
  uint8_t digest[32];
  uint32_t abi = 1;
};
bool validPackageId(const char* name);
// All fields are mandatory, unique, and versioned. Failure clears output.
bool parsePackage(const char* bytes, size_t length, const char* directory, Package& out);
}  // namespace sdapps
