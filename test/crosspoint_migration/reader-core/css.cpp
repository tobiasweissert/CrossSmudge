#include <Epub/FootnoteEntry.h>
#include <Epub/ReaderRenderSpec.h>
#include <Epub/blocks/BlockStyle.h>
#include <Epub/css/CssParser.h>
#include <HalStorage.h>

#include <cassert>
#include <iostream>
static void put(const std::string& name, const std::string& text) { Storage.put(name, {text.begin(), text.end()}); }
int main() {
  const std::string css =
      "p {text-align:right; font-variant:small-caps; direction:rtl;} div.a > p.b {font-size:1.5em; border:2px solid; "
      "background-color:#ccc;} #z {text-align:center;} p::first-letter{float:left; initial-letter:3;} "
      "img{background-color:black;}";
  put("/style", css);
  HalFile file;
  assert(Storage.openFileForRead("t", "/style", file));
  CssParser parser("/cache");
  assert(parser.loadFromStream(file) == CssParser::ParseResult::Complete);
  auto ancestor = CssParser::makeAncestor("div", "a", "");
  auto s = parser.resolveStyle("p", "b", "z", &ancestor, 1);
  assert(s.textAlign == CssTextAlign::Center && s.smallCaps && s.direction == CssTextDirection::Rtl);
  assert(s.hasFontSize() && s.fontSize.value == 1.5f);
  assert(s.shaded && s.hasVisibleBorder());
  auto initial = parser.resolveStyle("p", "", {}, nullptr, 0, true);
  assert(initial.floatLeft && initial.initialLetter == 3 && parser.hasFirstLetterRules());
  auto black = parser.resolveStyle("img", "");
  assert(black.hasBackgroundBlack() && black.backgroundBlack);
  assert(parser.saveToCache(true));
  CssParser warm("/cache");
  assert(warm.inspectCache() == CssParser::CacheStatus::Complete);
  assert(warm.loadFromCache() == CssParser::CacheLoadResult::Complete);
  auto w = warm.resolveStyle("p", "b", "z", &ancestor, 1);
  assert(w.textAlign == s.textAlign && w.smallCaps && w.fontSize.value == s.fontSize.value && w.shaded &&
         w.hasVisibleBorder());
  auto b = warm.resolveStyle("img", "");
  assert(b.hasBackgroundBlack() && b.backgroundBlack);
  assert(parser.saveToCache(false));
  CssParser partial("/cache");
  assert(partial.inspectCache() == CssParser::CacheStatus::Partial);
  assert(partial.loadFromCache() == CssParser::CacheLoadResult::Complete);
  auto transparent = CssParser::parseInlineStyle("background-color:rgba(0,0,0,0)");
  assert(!transparent.backgroundBlack);
  auto opaque = CssParser::parseInlineStyle("background:rgba(0, 0, 0, 1.0) !important");
  assert(opaque.backgroundBlack && opaque.hasBackgroundBlack());
  auto clear = CssParser::parseInlineStyle("background-color:white; font-variant:normal");
  black.applyOver(clear);
  assert(!black.backgroundBlack && !black.smallCaps);
  auto inherited = BlockStyle::fromCssStyle(s, 20, CssTextAlign::Justify, 200);
  assert(inherited.isRtl && inherited.directionDefined);
  ReaderRenderSpec spec;
  auto sig = readerRenderSpecSignature(spec);
  spec.characterSpacing = 1;
  assert(sig != readerRenderSpecSignature(spec));
  spec = {};
  spec.wordSpacingPercent = 120;
  assert(sig != readerRenderSpecSignature(spec));
  spec = {};
  spec.paragraphIndentSpaces = 4;
  assert(sig != readerRenderSpecSignature(spec));
  assert(FOOTNOTE_HREF_LEN == 256 && EPUB_MAX_FOOTNOTES_PER_PAGE == 16);
  // A source bound reports partial, never a successfully completed prefix.
  std::string large(CssParser::maxSourceBytes() + 1024, ' ');
  put("/large", large);
  assert(Storage.openFileForRead("t", "/large", file));
  CssParser bounded("/largecache");
  assert(bounded.loadFromStream(file) == CssParser::ParseResult::Partial);
  std::cout << "PASS production CSS cold/warm/partial, selectors/dropcaps/RTL/style wire/source bounds and render "
               "signatures\n";
}
