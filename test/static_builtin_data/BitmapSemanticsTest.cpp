#include <cstdio>
#include <cstring>

#include "AllocationObserver.h"
#include "StaticBuiltinData.h"

static static_builtin::Context context;
static static_builtin::Backing<EpdGlyph, 3> backing;
static int failures;

// Stored zlib block, rebuilt with valid Adler and decoded CRC for each semantic
// mutation. Thus a rejection must survive successful real metadata decoding.
static void checkFont(const char* name, EpdFontData& font, EpdGlyph* glyphs, size_t count, size_t bitmapSize,
                      bool expected) {
  unsigned char packed[11 + sizeof(backing.values)]{};
  const size_t bytes = count * sizeof(EpdGlyph);
  packed[0] = 0x78;
  packed[1] = 0x01;
  packed[2] = 1;
  packed[3] = bytes & 255;
  packed[4] = bytes >> 8;
  packed[5] = (~bytes) & 255;
  packed[6] = ((~bytes) >> 8) & 255;
  std::memcpy(packed + 7, glyphs, bytes);
  const uint32_t adler = mz_adler32(1, packed + 7, bytes);
  for (size_t i = 0; i < 4; ++i) packed[7 + bytes + i] = adler >> (24 - i * 8);
  static_builtin::Block block{packed, 11 + bytes,      reinterpret_cast<unsigned char*>(&backing),
                              bytes,  sizeof(backing), static_cast<uint32_t>(mz_crc32(0, packed + 7, bytes))};
  font.glyph = backing.values;
  static_builtin::FontCheck check{&font, count, bitmapSize};
  context.phase = static_builtin::Phase::Empty;
  forbidAllocation = true;
  const bool result = static_builtin::hydrate(context, &block, 1, &check, 1, nullptr, 0);
  const bool terminal = context.phase == (expected ? static_builtin::Phase::Ready : static_builtin::Phase::Failed);
  const bool sticky = expected || !static_builtin::hydrate(context, nullptr, 0, nullptr, 0, nullptr, 0);
  forbidAllocation = false;
  if (result != expected || !terminal || !sticky) {
    std::printf("FAIL %s expected=%d actual=%d\n", name, expected, result);
    ++failures;
  } else {
    std::printf("PASS bitmap_%s\n", name);
  }
}

int main() {
  static const uint8_t bitmap[8]{};
  EpdUnicodeInterval interval{32, 32, 0};
  EpdFontData font{};
  font.bitmap = bitmap;
  font.intervals = &interval;
  font.intervalCount = 1;
  EpdGlyph glyphs[3]{};
  glyphs[0] = {255, 255, 0, 0, 0, 1, 0};
  checkFont("raw_1bit_dimensions", font, glyphs, 1, 1, false);
  font.is2Bit = true;
  checkFont("raw_2bit_dimensions", font, glyphs, 1, 1, false);
  glyphs[0] = {3, 3, 0, 0, 0, 3, 5};
  checkFont("raw_2bit_exact_tail", font, glyphs, 1, 8, true);
  glyphs[0].dataLength = 2;
  checkFont("raw_2bit_short_length", font, glyphs, 1, 8, false);
  glyphs[0].dataLength = 3;
  glyphs[0].dataOffset = 6;
  checkFont("raw_2bit_offset", font, glyphs, 1, 8, false);
  font.is2Bit = false;
  glyphs[0] = {3, 3, 0, 0, 0, 2, 6};
  checkFont("raw_1bit_exact_tail", font, glyphs, 1, 8, true);
  glyphs[0].dataLength = 1;
  checkFont("raw_1bit_short_length", font, glyphs, 1, 8, false);
  glyphs[0] = {0, 255, 0, 0, 0, 0, 8};
  checkFont("raw_empty_glyph", font, glyphs, 1, 8, true);
  font.bitmap = nullptr;
  checkFont("null_bitmap", font, glyphs, 1, 8, false);
  font.bitmap = bitmap;
  glyphs[0] = {1, 1, 0, 0, 0, 1, 0xffffffffu};
  checkFont("raw_overflow_offset", font, glyphs, 1, 8, false);
  interval.last = 34;
  font.is2Bit = true;
  glyphs[0] = {1, 4, 0, 0, 0, 1, 0};
  glyphs[1] = {1, 4, 0, 0, 0, 1, 0};
  glyphs[2] = {1, 4, 0, 0, 0, 1, 1};
  EpdFontGroup groups[] = {{0, 4, 8, 2, 0}, {4, 4, 4, 1, 1}};
  uint16_t mapping[] = {0, 1, 0};
  font.groups = groups;
  font.groupCount = 2;
  font.glyphToGroup = mapping;
  checkFont("frequency_noncontiguous_valid", font, glyphs, 3, 8, true);
  groups[0].uncompressedSize = 7;
  checkFont("group_cumulative_row_footprint", font, glyphs, 3, 8, false);
  groups[0].uncompressedSize = 8;
  groups[1].uncompressedSize = 1;
  checkFont("group_individual_row_footprint", font, glyphs, 3, 8, false);
  groups[1].uncompressedSize = 4;
  groups[0].glyphCount = 1;
  checkFont("frequency_counted_membership", font, glyphs, 3, 8, false);
  groups[0].glyphCount = 2;
  mapping[2] = 2;
  checkFont("frequency_invalid_mapping", font, glyphs, 3, 8, false);
  mapping[2] = 0;
  glyphs[2].dataOffset = 8;
  checkFont("group_offset", font, glyphs, 3, 8, false);
  glyphs[2].dataOffset = 1;
  glyphs[0] = {255, 255, 0, 0, 0, 1, 0};
  checkFont("group_dimensions", font, glyphs, 3, 8, false);
  glyphs[0] = {3, 3, 0, 0, 0, 2, 0};
  font.is2Bit = false;
  checkFont("group_compactor_2bit_scratch_even_if_1bit", font, glyphs, 3, 8, false);
  font.is2Bit = true;
  glyphs[0] = {1, 4, 0, 0, 0, 1, 0};
  font.glyphToGroup = nullptr;
  groups[1].firstGlyphIndex = 2;
  checkFont("contiguous_valid", font, glyphs, 3, 8, true);
  groups[1].firstGlyphIndex = 1;
  checkFont("contiguous_overlap", font, glyphs, 3, 8, false);
  groups[1].firstGlyphIndex = 0xffffffffu;
  checkFont("group_first_overflow", font, glyphs, 3, 8, false);
  groups[1].firstGlyphIndex = 2;
  groups[1].compressedOffset = 0xffffffffu;
  checkFont("group_compressed_overflow", font, glyphs, 3, 8, false);
  return failures ? 1 : 0;
}
