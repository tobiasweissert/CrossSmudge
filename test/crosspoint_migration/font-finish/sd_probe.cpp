#include <HalStorage.h>
#include <SdCardFont.h>

#include <cassert>
#include <cstring>
#include <deque>
#include <string>
static void put16(size_t p, uint16_t v) {
  sdFontTestFile[p] = v;
  sdFontTestFile[p + 1] = v >> 8;
}
static void put32(size_t p, uint32_t v) {
  put16(p, v);
  put16(p + 2, v >> 16);
}
void makeKerningFont(uint16_t extraEntries = 0, uint8_t classCount = 2, uint8_t extraClass = 2, bool ligature = false) {
  constexpr uint32_t KERN_GLYPHS = 7;
  constexpr size_t GLYPH_OFFSET = 64 + 24;
  constexpr size_t KERN_OFFSET = GLYPH_OFFSET + KERN_GLYPHS * sizeof(EpdGlyph);
  constexpr uint8_t LEFT[][2] = {{'A', 1}, {'C', 2}};
  constexpr uint8_t RIGHT[][2] = {{'B', 1}, {'D', 2}};
  constexpr int8_t MATRIX[2][2] = {{-3, 0}, {4, -5}};
  const uint16_t entries = classCount ? 2 + extraEntries : 0;
  const size_t matrixBytes = static_cast<size_t>(classCount) * classCount;
  const size_t ligatureOffset = KERN_OFFSET + entries * 3 * 2 + matrixBytes;
  const size_t bitmapOffset = ligatureOffset + (ligature ? 8 : 0);
  sdFontTestFile.assign(bitmapOffset + KERN_GLYPHS * 128, 0);
  std::memcpy(sdFontTestFile.data(), "CPFONT\0\0", 8);
  put16(8, CPFONT_VERSION);
  sdFontTestFile[12] = 1;
  put32(36, 2);
  put32(40, KERN_GLYPHS);
  sdFontTestFile[44] = 32;
  put16(45, 32);
  put16(49, entries);  // left class entries
  put16(51, entries);  // right class entries
  sdFontTestFile[53] = classCount;
  sdFontTestFile[54] = classCount;
  sdFontTestFile[55] = ligature ? 1 : 0;
  put32(56, 64);
  put32(64, 'A');
  put32(68, 'F');
  put32(76, 0xFFFD);
  put32(80, 0xFFFD);
  put32(84, KERN_GLYPHS - 1);
  for (uint32_t i = 0; i < KERN_GLYPHS; ++i) {
    EpdGlyph glyph{};
    glyph.width = 32;
    glyph.height = 32;
    glyph.advanceX = 32 << 4;
    glyph.top = 32;
    glyph.dataLength = 128;
    glyph.dataOffset = i * 128;
    std::memcpy(sdFontTestFile.data() + GLYPH_OFFSET + i * sizeof(glyph), &glyph, sizeof(glyph));
  }
  size_t at = KERN_OFFSET;
  for (const auto* table : {LEFT, RIGHT}) {
    if (classCount == 0) break;
    for (size_t i = 0; i < 2; ++i, at += 3) {
      put16(at, table[i][0]);
      sdFontTestFile[at + 2] = table[i][1];
    }
    for (uint16_t i = 0; i < extraEntries; ++i, at += 3) {
      put16(at, 0x100 + i);
      sdFontTestFile[at + 2] = extraClass ? extraClass : 3 + i % (classCount - 2);
    }
  }
  for (size_t row = 0; classCount > 0 && row < 2; ++row) {
    std::memcpy(sdFontTestFile.data() + at + row * classCount, MATRIX[row], sizeof(MATRIX[row]));
  }
  if (ligature) {
    put32(ligatureOffset, 'E' << 16 | 'F');
    put32(ligatureOffset + 4, 'A');
  }
}
int main() {
  constexpr size_t off = 88, bitmap = off + 3 * sizeof(EpdGlyph);
  sdFontTestFile.assign(bitmap + 3, 0);
  memcpy(sdFontTestFile.data(), "CPFONT\0\0", 8);
  put16(8, CPFONT_VERSION);
  sdFontTestFile[12] = 1;
  put32(36, 2);
  put32(40, 3);
  sdFontTestFile[44] = 8;
  put16(45, 8);
  put32(56, 64);
  put32(64, 'A');
  put32(68, 'B');
  put32(76, 0xFFFD);
  put32(80, 0xFFFD);
  put32(84, 2);
  for (unsigned i = 0; i < 3; ++i) {
    EpdGlyph g{};
    g.width = 2;
    g.height = 2;
    g.top = 2;
    g.advanceX = 72 + i * 16;
    g.dataOffset = i;
    g.dataLength = 1;
    memcpy(sdFontTestFile.data() + off + i * sizeof(g), &g, sizeof(g));
    sdFontTestFile[bitmap + i] = 0xF0 + i;
  }
  SdCardFont font;
  assert(font.load("/font.cpfont"));
  uint16_t advance = 0;
  assert(font.readAdvance('A', 0, &advance) && advance == 72);
  const char packed[] = "A\0B\0";
  const char* seg[] = {packed};
  size_t len[] = {4};
  assert(font.buildAdvanceTablePacked(seg, len, 1, false, false, 1) == 0);
  assert(font.getAdvance('B', 0) == 88);
  assert(font.prewarm("AB", 1) == 0);
  assert(!font.lastPrewarmFailed());
  const auto* data = font.getEpdFont()->data;
  assert(data->bitmap && data->bitmap[0] == 0xF0);
  font.releaseForLowMemory(true);
  assert(font.hasAdvanceTable());
  assert(font.readAdvance('B', 0, &advance) && advance == 88);
  assert(font.prewarm("A", 1, false, false) == 0);
  font.releaseResidentCaches();
  assert(!font.hasAdvanceTable());
  assert(font.prewarm("B", 1) == 0);
  assert(font.getEpdFont()->findGlyph('B'));
  std::deque<std::string> words = {"A", "B"};
  assert(font.buildAdvanceTable(words, false, 1) == 0);
  makeKerningFont(0, 2, 2, true);
  SdCardFont lean;
  assert(lean.load("/lean.cpfont"));
  assert(lean.prewarm("EFAB", 1, false, false) == 0);
  assert(lean.getEpdFont()->data->ligaturePairCount == 1);
  assert(lean.getEpdFont()->data->kernLeftEntryCount == 0);
  assert(lean.prewarm("EFAB", 1, false, true) == 0);
  assert(lean.getEpdFont()->data->kernLeftEntryCount > 0);
  assert(lean.prewarm("EFAB", 1, false, false) == 0);
  assert(lean.getEpdFont()->data->kernLeftEntryCount == 0);
  assert(lean.getEpdFont()->data->ligaturePairCount == 1);
  const auto beforeHash = lean.contentHash();
  sdFontTestFile.back() ^= 0x01;
  SdCardFont replacement;
  assert(replacement.load("/lean.cpfont"));
  assert(replacement.contentHash() != beforeHash);
}
