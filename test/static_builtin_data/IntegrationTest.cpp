#include <cassert>
#include <cstdio>
#include <cstring>

#include "I18n.h"
#include "I18nStrings.h"
#ifdef STATIC_BUILTIN_HOST_TEST
#include "AllocationObserver.h"
#include "EpdFont.h"
#include "EpdFontFamily.h"
#include "StaticBuiltinData.h"
#include "StaticBuiltinFonts.h"
static static_builtin::Context failedContext;
static const EpdFont earlyFont(&ui_symbols_10);
static const EpdFontFamily earlyFamily(&earlyFont);
#endif

static void printHex(const char* text) {
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) std::printf("%02x", *p);
  std::putchar('\n');
}
int main() {
#ifdef STATIC_BUILTIN_HOST_TEST
  assert(!static_builtin::builtinsReady());
  assert(earlyFamily.getSizeInPixels() == ui_symbols_10.ascender - ui_symbols_10.descender);
  const auto ft = static_builtin::fontTables();
  const auto lt = static_builtin::languageTables();
  assert(ft.blockCount == 76 && ft.fontCount == 38 && lt.blockCount == 34 && lt.languageCount == 35);
  assert(ft.fonts[0].font == &ui_symbols_10);  // no second raw descriptor/bitmap graph
  // First table decodes correctly, last table fails identity: no Ready publication.
  auto bad = lt.blocks[lt.blockCount - 1];
  bad.crc ^= 1;
  static_builtin::Tables failTables[] = {ft, lt, {&bad, 1, nullptr, 0, nullptr, 0}};
  forbidAllocation = true;
  assert(!static_builtin::hydrateAll(failedContext, failTables, 3));
  assert(failedContext.phase == static_builtin::Phase::Failed);
  assert(!static_builtin::hydrateAll(failedContext, failTables, 3));
  assert(!static_builtin::builtinsReady());
  assert(static_builtin::hydrateBuiltins());
  assert(static_builtin::builtinsReady());
  assert(static_builtin::hydrateBuiltins());
  forbidAllocation = false;
  std::fprintf(stderr,
               "PASS transaction_all_110_blocks_and_terminal_failure host_decoder_size=%zu host_context_size=%zu\n",
               sizeof(tinfl_decompressor), sizeof(static_builtin::Context));
  // Exercise the unchanged public lookup implementation, not a test lookup clone.
  const EpdGlyph* retainedGlyphs[38]{};
  for (size_t i = 0; i < ft.fontCount; ++i) {
    const EpdFont font(ft.fonts[i].font);
    const auto cp = font.data->intervals[0].first;
    forbidAllocation = true;
    retainedGlyphs[i] = font.findGlyph(cp);
    assert(retainedGlyphs[i] == font.data->glyph);
    assert(font.hasCodepoint(cp));
    assert(font.getGlyph(cp) == retainedGlyphs[i]);
    for (size_t j = 0; j < font.data->intervalCount; ++j) {
      const auto& interval = font.data->intervals[j];
      for (uint32_t codepoint = interval.first; codepoint <= interval.last; ++codepoint) {
        assert(font.findGlyph(codepoint) == font.data->glyph + interval.offset + codepoint - interval.first);
        assert(font.hasCodepoint(codepoint));
      }
    }
    assert(!font.findGlyph(0x110000));
    assert(!font.hasCodepoint(0x110000));
    forbidAllocation = false;
  }
#endif
  auto& i18n = I18n::getInstance();
  static const char* retained[static_cast<size_t>(Language::_COUNT)][static_cast<size_t>(StrId::_COUNT)]{};
  for (unsigned lang = 0; lang < getLanguageCount(); ++lang) {
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = true;
#endif
    i18n.setLanguage(static_cast<Language>(lang));
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = false;
#endif
    printHex(i18n.getLanguageName(static_cast<Language>(lang)));
    printHex(LANGUAGE_CODES[lang]);
    printHex(CHARACTER_SETS[lang]);
    for (unsigned id = 0; id < static_cast<unsigned>(StrId::_COUNT); ++id) {
#ifdef STATIC_BUILTIN_HOST_TEST
      forbidAllocation = true;
#endif
      retained[lang][id] = i18n.get(static_cast<StrId>(id));
#ifdef STATIC_BUILTIN_HOST_TEST
      forbidAllocation = false;
#endif
      printHex(retained[lang][id]);
    }
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = true;
#endif
    for (unsigned id = static_cast<unsigned>(StrId::_COUNT); id <= 65535; ++id)
      assert(std::strcmp(i18n.get(static_cast<StrId>(id)), "???") == 0);
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = false;
#endif
  }
  for (unsigned lang = 0; lang < getLanguageCount(); ++lang) {
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = true;
#endif
    i18n.setLanguage(static_cast<Language>(lang));
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = false;
#endif
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = true;
#endif
    for (unsigned id = 0; id < static_cast<unsigned>(StrId::_COUNT); ++id)
      assert(i18n.get(static_cast<StrId>(id)) == retained[lang][id]);
#ifdef STATIC_BUILTIN_HOST_TEST
    for (size_t i = 0; i < ft.fontCount; ++i)
      assert(EpdFont(ft.fonts[i].font).findGlyph(ft.fonts[i].font->intervals[0].first) == retainedGlyphs[i]);
    forbidAllocation = false;
#endif
  }
  const auto previous = i18n.getLanguage();
  for (unsigned lang = getLanguageCount(); lang <= 255; ++lang) {
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = true;
#endif
    i18n.setLanguage(static_cast<Language>(lang));
#ifdef STATIC_BUILTIN_HOST_TEST
    forbidAllocation = false;
#endif
    assert(i18n.getLanguage() == previous);
    assert(std::strcmp(i18n.getLanguageName(static_cast<Language>(lang)), "???") == 0);
    assert(getLanguageStrings(static_cast<Language>(lang)).data == i18n_strings::STRINGS_EN_DATA);
  }
  assert(I18n::languageFromCode("invalid") == Language::EN);
  std::fprintf(stderr, "PASS all_language_x_strid_raw_equivalence_and_retained_pointers languages=%u strids=%u\n",
               getLanguageCount(), static_cast<unsigned>(StrId::_COUNT));
}
