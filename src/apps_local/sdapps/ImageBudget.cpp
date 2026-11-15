#include "ImageBudget.h"
#include "Package.h"
#include <cstring>
#include <limits>

namespace sdapps {
namespace {
uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
bool range(size_t offset, size_t length, size_t size) { return offset <= size && length <= size - offset; }
}
bool imageWithinBudget(const uint8_t* b, size_t size) {
  constexpr uint32_t MAX_LOADED = 2 * 1024 * 1024;
  constexpr uint32_t MAX_RELOCATIONS = 16384;
  constexpr uint32_t MAX_SYMBOLS = 4096;
  if (!b || size < 52 || size > MAX_IMAGE_BYTES || memcmp(b, "\177ELF\1\1\1", 7) != 0) return false;
  if (u16(b + 16) != 3 || u16(b + 18) != 94 || u32(b + 20) != 1 || u16(b + 40) != 52) return false;
  const uint32_t shoff = u32(b + 32);
  const uint16_t shnum = u16(b + 48);
  const uint16_t phnum = u16(b + 44);
  if (!shnum || shnum > 96 || u16(b + 46) != 40 || !range(shoff, size_t(shnum) * 40, size)) return false;
  if (phnum > 16 || (phnum && (u16(b + 42) != 32 || !range(u32(b + 28), size_t(phnum) * 32, size)))) return false;
  uint32_t allocated = 0, relocations = 0, symbols = 0, imports = 0;
  uint32_t low = std::numeric_limits<uint32_t>::max(), high = 0;
  for (uint16_t i = 0; i < shnum; ++i) {
    const uint8_t* s = b + shoff + size_t(i) * 40;
    const uint32_t type = u32(s + 4), flags = u32(s + 8), address = u32(s + 12);
    const uint32_t offset = u32(s + 16), length = u32(s + 20);
    if (type != 8 && !range(offset, length, size)) return false;
    if (flags & 2) {
      if (length > MAX_LOADED - allocated || length > UINT32_MAX - address) return false;
      allocated += length;
      if (length) {
        if (address < low) low = address;
        if (address + length > high) high = address + length;
      }
    }
    switch (type) {
      case 4:
        if (u32(s + 36) != 12 || length % 12 || length / 12 > MAX_RELOCATIONS - relocations) return false;
        relocations += length / 12;
        break;
      case 2:
      case 11:
        if (u32(s + 36) != 16 || length % 16 || length / 16 > MAX_SYMBOLS - symbols) return false;
        symbols += length / 16;
        for (uint32_t j = 0; j < length; j += 16) {
          const uint8_t* symbol = b + offset + j;
          if (u32(symbol) && !u16(symbol + 14) && ++imports > 32) return false;
        }
        break;
      case 9:
        return false;
      default:
        break;  // Other section kinds are checked by the structural validator.
    }
  }
  return allocated && high >= low && high - low <= MAX_LOADED;
}
}  // namespace sdapps
