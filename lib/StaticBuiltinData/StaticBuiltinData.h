#pragma once
// Private fixed-storage experiment. No public font or I18n ABI changes.
#if defined(STATIC_BUILTIN_HOST_TEST) && defined(CROSSPOINT_STATIC_BUILTINS)
#error "host test must not masquerade as target qualification"
#endif
#if defined(CROSSPOINT_STATIC_BUILTINS)
#include "esp_attr.h"
#include "sdkconfig.h"
#if !defined(FREEINK_DEVICE_STICKY) || !FREEINK_DEVICE_STICKY || defined(SIMULATOR) ||                       \
    !defined(CONFIG_IDF_TARGET_ESP32S3) || !CONFIG_IDF_TARGET_ESP32S3 ||                                     \
    !defined(CONFIG_ESP_ALLOW_BSS_SEG_EXTERNAL_MEMORY) || !CONFIG_ESP_ALLOW_BSS_SEG_EXTERNAL_MEMORY ||       \
    !defined(CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY) || !CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY || \
    !defined(CONFIG_SPIRAM_BOOT_INIT) || !CONFIG_SPIRAM_BOOT_INIT ||                                         \
    (defined(CONFIG_SPIRAM_IGNORE_NOTFOUND) && CONFIG_SPIRAM_IGNORE_NOTFOUND)
#error "static builtins require supported early mandatory PSRAM external BSS configuration"
#endif
#define STATIC_BUILTIN_BSS EXT_RAM_BSS_ATTR
#elif defined(STATIC_BUILTIN_HOST_TEST)
#define STATIC_BUILTIN_BSS
#else
#error "StaticBuiltinData is opt-in only; refusing internal-RAM fallback"
#endif

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "EpdFontData.h"
#include "MinizConfig.h"

namespace static_builtin {
#if defined(CROSSPOINT_STATIC_BUILTINS)
static_assert(sizeof(void*) == 4 && sizeof(size_t) == 4, "target pointer ABI");
static_assert(sizeof(tinfl_decompressor) == 8364 && alignof(tinfl_decompressor) == 4,
              "target decoder ABI requires review (host is not target proof)");
#endif
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "unsupported endian");
static_assert(std::is_same<decltype(EpdGlyph::width), uint8_t>::value &&
                  std::is_same<decltype(EpdGlyph::height), uint8_t>::value &&
                  std::is_same<decltype(EpdGlyph::advanceX), uint16_t>::value &&
                  std::is_same<decltype(EpdGlyph::left), int16_t>::value &&
                  std::is_same<decltype(EpdGlyph::top), int16_t>::value &&
                  std::is_same<decltype(EpdGlyph::dataLength), uint16_t>::value &&
                  std::is_same<decltype(EpdGlyph::dataOffset), uint32_t>::value,
              "glyph field types");
static_assert(std::is_same<decltype(EpdUnicodeInterval::first), uint32_t>::value &&
                  std::is_same<decltype(EpdUnicodeInterval::last), uint32_t>::value &&
                  std::is_same<decltype(EpdUnicodeInterval::offset), uint32_t>::value,
              "interval field types");
static_assert(sizeof(EpdGlyph) == 16 && alignof(EpdGlyph) == 4, "glyph ABI");
static_assert(offsetof(EpdGlyph, width) == 0 && offsetof(EpdGlyph, height) == 1 && offsetof(EpdGlyph, advanceX) == 2 &&
                  offsetof(EpdGlyph, left) == 4 && offsetof(EpdGlyph, top) == 6 &&
                  offsetof(EpdGlyph, dataLength) == 8 && offsetof(EpdGlyph, dataOffset) == 12,
              "glyph offsets");
static_assert(sizeof(EpdUnicodeInterval) == 12 && alignof(EpdUnicodeInterval) == 4 &&
                  offsetof(EpdUnicodeInterval, first) == 0 && offsetof(EpdUnicodeInterval, last) == 4 &&
                  offsetof(EpdUnicodeInterval, offset) == 8,
              "interval ABI");
static_assert(std::is_trivially_copyable<EpdGlyph>::value && std::is_standard_layout<EpdGlyph>::value &&
                  std::is_trivially_copyable<EpdUnicodeInterval>::value &&
                  std::is_standard_layout<EpdUnicodeInterval>::value,
              "typed representation lifetime");

template <typename T, size_t N>
struct alignas(4) Backing {
  static_assert(N > 0 && std::is_trivial<T>::value && std::is_standard_layout<T>::value, "typed backing");
  T values[N];
  unsigned char terminal;
};
struct Block {
  const unsigned char* input;
  size_t inputSize;
  unsigned char* object;
  size_t decodedSize;
  size_t objectSize;
  uint32_t crc;
};
struct FontCheck {
  const EpdFontData* font;
  size_t glyphCount;
  size_t bitmapSize;
};
struct LanguageCheck {
  const char* data;
  size_t size;
  const uint16_t* offsets;
  size_t count;
  const char* english;
  size_t englishSize;
  const uint16_t* englishOffsets;
  bool isEnglish;
};
enum class Phase : uint8_t { Empty, Working, Ready, Failed };
struct Context {
  tinfl_decompressor decoder;
  Phase phase;
};
struct Tables {
  const Block* blocks;
  size_t blockCount;
  const FontCheck* fonts;
  size_t fontCount;
  const LanguageCheck* languages;
  size_t languageCount;
};
Tables fontTables();
Tables languageTables();
bool hydrateBuiltins();
bool builtinsReady();
// Single boot owner only. A failure is terminal; backing is private until Ready.
// No consumers, including UI/error routes, may execute before successful return.
bool hydrateAll(Context&, const Tables*, size_t);
bool hydrate(Context&, const Block*, size_t, const FontCheck*, size_t, const LanguageCheck*, size_t);
}  // namespace static_builtin
