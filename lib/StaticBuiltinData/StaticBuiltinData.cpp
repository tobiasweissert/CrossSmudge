#if defined(CROSSPOINT_STATIC_BUILTINS) || defined(STATIC_BUILTIN_HOST_TEST)
#if defined(CROSSPOINT_STATIC_BUILTINS)
#include "sdkconfig.h"
#if !defined(CONFIG_SPIRAM_BOOT_HW_INIT) || !CONFIG_SPIRAM_BOOT_HW_INIT || !defined(CONFIG_SPIRAM_BOOT_INIT) ||    \
    !CONFIG_SPIRAM_BOOT_INIT || !defined(CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION) ||                         \
    !CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION || !defined(CONFIG_SPIRAM_MEMTEST) || !CONFIG_SPIRAM_MEMTEST || \
    !defined(CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY) || !CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY ||       \
    !defined(CONFIG_ESP_ALLOW_BSS_SEG_EXTERNAL_MEMORY) || !CONFIG_ESP_ALLOW_BSS_SEG_EXTERNAL_MEMORY ||             \
    (defined(CONFIG_SPIRAM_IGNORE_NOTFOUND) && CONFIG_SPIRAM_IGNORE_NOTFOUND)
#error "static builtins require supported early mandatory PSRAM external BSS configuration"
#endif
#endif
#include <cstring>

#include "StaticBuiltinData.h"

namespace static_builtin {
namespace {
bool validFont(const FontCheck& check) {
  if (!check.font || !check.glyphCount) return false;
  const auto& f = *check.font;
  if (!f.bitmap || !f.glyph || !f.intervals || !f.intervalCount) return false;
  size_t coverage = 0;
  uint32_t last = 0;
  for (size_t i = 0; i < f.intervalCount; ++i) {
    const auto& interval = f.intervals[i];
    if (interval.first > interval.last || interval.last > 0x10ffff || (i && interval.first <= last) ||
        interval.offset != coverage)
      return false;
    const size_t span = size_t(interval.last) - interval.first + 1;
    if (coverage > check.glyphCount || span > check.glyphCount - coverage) return false;
    coverage += span;
    last = interval.last;
  }
  if (coverage != check.glyphCount || bool(f.groups) != bool(f.groupCount) || f.groupCount > check.glyphCount)
    return false;
  for (size_t i = 0; i < f.groupCount; ++i) {
    const auto& group = f.groups[i];
    if (group.compressedOffset > check.bitmapSize || group.compressedSize > check.bitmapSize - group.compressedOffset ||
        !group.glyphCount || group.firstGlyphIndex > check.glyphCount ||
        group.glyphCount > check.glyphCount - group.firstGlyphIndex)
      return false;
    // FontDecompressor::getAlignedOffset/compactSingleGlyph always consume
    // 2-bit, byte-aligned rows. Frequency membership is NOT a contiguous range.
    // Scan without scratch allocation; O(groupCount * glyphCount), both bounded
    // by the validated coverage and the trusted generated descriptor inventory.
    size_t members = 0;
    size_t footprint = 0;
    for (size_t j = 0; j < check.glyphCount; ++j) {
      const bool member = f.glyphToGroup ? f.glyphToGroup[j] == i
                                         : j >= group.firstGlyphIndex && j - group.firstGlyphIndex < group.glyphCount;
      if (!member) continue;
      ++members;
      const auto& glyph = f.glyph[j];
      const size_t aligned = ((size_t(glyph.width) + 3) / 4) * glyph.height;
      if (footprint > group.uncompressedSize || aligned > group.uncompressedSize - footprint) return false;
      footprint += aligned;
    }
    if (members != group.glyphCount) return false;
  }
  for (size_t i = 0; i < check.glyphCount; ++i) {
    const auto& glyph = f.glyph[i];
    size_t limit = check.bitmapSize;
    if (f.groups) {
      size_t groupIndex = f.groupCount;
      if (f.glyphToGroup)
        groupIndex = f.glyphToGroup[i];
      else {
        for (size_t j = 0; j < f.groupCount; ++j) {
          const auto& group = f.groups[j];
          if (i >= group.firstGlyphIndex && i - group.firstGlyphIndex < group.glyphCount) {
            if (groupIndex != f.groupCount) return false;
            groupIndex = j;
          }
        }
      }
      if (groupIndex >= f.groupCount) return false;
      limit = f.groups[groupIndex].uncompressedSize;
    }
    // GfxRenderer indexes a continuous packed pixel stream, not padded rows.
    // uint8 dimensions bound the product to 65025 (also on the 32-bit target).
    const size_t pixels = size_t(glyph.width) * glyph.height;
    const size_t perByte = (f.groups || f.is2Bit) ? 4 : 8;
    const size_t packed = (pixels + perByte - 1) / perByte;
    if (glyph.dataOffset > limit || glyph.dataLength > limit - glyph.dataOffset || packed > glyph.dataLength)
      return false;
  }
  return true;
}
bool stringAt(const char* data, size_t size, size_t offset) {
  return data && offset < size && (offset == 0 || data[offset - 1] == '\0') &&
         std::memchr(data + offset, 0, size - offset);
}
bool validLanguage(const LanguageCheck& language) {
  if (!language.offsets || !language.englishOffsets || !language.count || !language.size || language.size > 32768 ||
      !language.englishSize || language.englishSize > 32768)
    return false;
  for (size_t i = 0; i < language.count; ++i) {
    const uint16_t offset = language.offsets[i];
    const uint16_t en = language.englishOffsets[i];
    if ((en & 0x8000) || !stringAt(language.english, language.englishSize, en)) return false;
    if (offset & 0x8000) {
      if (language.isEnglish || (offset & 0x7fff) != en) return false;
    } else if (!stringAt(language.data, language.size, offset))
      return false;
  }
  return true;
}
}  // namespace
bool hydrate(Context& context, const Block* blocks, size_t count, const FontCheck* fonts, size_t fontCount,
             const LanguageCheck* languages, size_t languageCount) {
  const Tables table{blocks, count, fonts, fontCount, languages, languageCount};
  return hydrateAll(context, &table, 1);
}
bool hydrateAll(Context& context, const Tables* tables, size_t tableCount) {
  if (context.phase != Phase::Empty) return context.phase == Phase::Ready;
  context.phase = Phase::Working;
  if (tableCount && !tables) {
    context.phase = Phase::Failed;
    return false;
  }
  for (size_t t = 0; t < tableCount; ++t) {
    const auto& table = tables[t];
    if ((table.blockCount && !table.blocks) || (table.fontCount && !table.fonts) ||
        (table.languageCount && !table.languages)) {
      context.phase = Phase::Failed;
      return false;
    }
    for (size_t i = 0; i < table.blockCount; ++i) {
      const auto& b = table.blocks[i];
      if (!b.input || !b.inputSize || !b.object || !b.decodedSize || b.decodedSize >= b.objectSize) {
        context.phase = Phase::Failed;
        return false;
      }
      std::memset(b.object + b.decodedSize, 0xa5, b.objectSize - b.decodedSize);
      tinfl_init(&context.decoder);
      size_t in = b.inputSize, out = b.decodedSize + 1;
      const auto status = tinfl_decompress(&context.decoder, b.input, &in, b.object, b.object, &out,
                                           TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
      if (status != TINFL_STATUS_DONE || in != b.inputSize || out != b.decodedSize || b.object[b.decodedSize] != 0xa5 ||
          mz_crc32(0, b.object, b.decodedSize) != b.crc) {
        context.phase = Phase::Failed;
        return false;
      }
    }
  }
  for (size_t t = 0; t < tableCount; ++t) {
    const auto& table = tables[t];
    for (size_t i = 0; i < table.fontCount; ++i) {
      if (!validFont(table.fonts[i])) {
        context.phase = Phase::Failed;
        return false;
      }
    }
    for (size_t i = 0; i < table.languageCount; ++i) {
      if (!validLanguage(table.languages[i])) {
        context.phase = Phase::Failed;
        return false;
      }
    }
  }
  context.phase = Phase::Ready;
  return true;
}
}  // namespace static_builtin
#if defined(CROSSPOINT_STATIC_BUILTINS)
// Private generated source included once: no second raw font graph.
#include "StaticBuiltinFonts.cpp"
#include "StaticBuiltinOwner.cpp"
#endif
#endif
