#include "TextBlock.h"

#include <BidiUtils.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Memory.h>
#include <MemoryManager.h>
#include <Serialization.h>

#include <cstring>
#include <string_view>

#include "../../../../src/fontIds.h"
#include "../CjkBreaks.h"

size_t TextBlock::arenaSize(const uint16_t wordCount, const bool hasFocus, const uint16_t textBytes) {
  // Layout documented in TextBlock.h: 16-bit arrays first, then 8-bit arrays, then text.
  size_t size =
      static_cast<size_t>(wordCount) * (sizeof(SourceRange) + sizeof(uint16_t) + sizeof(int16_t) + sizeof(uint8_t));
  if (hasFocus) {
    size += static_cast<size_t>(wordCount) * (sizeof(uint16_t) + sizeof(uint8_t));
  }
  return size + wordCount * 4U + textBytes;
}

std::unique_ptr<TextBlock> TextBlock::clone() const {
  if (!isValid) return nullptr;
  std::unique_ptr<TextBlock> copy(new (std::nothrow) TextBlock());
  if (!copy) return nullptr;
  copy->blockStyle = blockStyle;
  copy->numWords = numWords;
  copy->paragraphStartWord = paragraphStartWord;
  copy->textBytes = textBytes;
  copy->focusPresent = focusPresent;
  copy->layoutReferenceOffset = layoutReferenceOffset;
  if (numWords != 0) {
    const size_t bytes = arenaSize(numWords, focusPresent, textBytes);
    copy->arena = makeUniqueNoThrow<uint8_t[]>(bytes);
    if (!copy->arena) return nullptr;
    std::memcpy(copy->arena.get(), arena.get(), bytes);
    copy->bindArenaPointers();
  }
  if (wordFontData) {
    const size_t bytes = sizeof(int32_t) * MAX_WORD_FONTS + numWords;
    copy->wordFontData = makeUniqueNoThrow<uint8_t[]>(bytes);
    if (!copy->wordFontData) return nullptr;
    std::memcpy(copy->wordFontData.get(), wordFontData.get(), bytes);
  }
  copy->rubyTexts = rubyTexts;
  copy->linkSpans = linkSpans;
  return copy;
}

void TextBlock::bindArenaPointers() {
  uint8_t* base = arena.get();
  const size_t wc = numWords;
  sourceRanges = reinterpret_cast<const SourceRange*>(base);
  base += wc * sizeof(SourceRange);
  textOffArr = reinterpret_cast<const uint16_t*>(base);
  xposArr = reinterpret_cast<const int16_t*>(base + wc * 2);
  size_t off = wc * 4;
  if (focusPresent) {
    focusSplitXArr = reinterpret_cast<const uint16_t*>(base + off);
    off += wc * 2;
  }
  guideDots = reinterpret_cast<const uint16_t*>(base + off);
  off += wc * 2;
  flags = base + off;
  off += wc;
  spaces = base + off;
  off += wc;
  stylesArr = base + off;
  off += wc;
  if (focusPresent) {
    focusBoundaryArr = base + off;
    off += wc;
  }
  textArr = reinterpret_cast<const char*>(base + off);
}

TextBlock::TextBlock(const std::vector<std::string>& words, const std::vector<int16_t>& wordXpos,
                     const std::vector<EpdFontFamily::Style>& wordStyles, const std::vector<uint8_t>& focusBoundary,
                     const std::vector<uint16_t>& focusSplitX, const BlockStyle& blockStyle,
                     std::vector<std::string> rubyTexts, std::vector<LinkSpan> linkSpans,
                     const std::vector<SourceRange>& ranges, const uint16_t paragraphStartWord)
    : blockStyle(blockStyle),
      paragraphStartWord(paragraphStartWord),
      rubyTexts(std::move(rubyTexts)),
      linkSpans(std::move(linkSpans)) {
  // Same invariant as deserialize(): a block never holds an all-empty rubyTexts, so a
  // ruby-less line costs nothing beyond its arena. The layout engine hands one over for
  // every line it extracts, ruby or not; release it here rather than carrying it for the
  // block's lifetime. Move-assigning an empty vector frees the buffer (clear() would not).
  if (!hasRuby()) {
    this->rubyTexts = std::vector<std::string>{};
  }

  // Focus annotations are optional: empty vectors mean no word in this block has a split.
  // When present, they must be sized in lockstep with words[].
  const bool hasFocus = !focusBoundary.empty();
  if ((!ranges.empty() && ranges.size() != words.size()) || words.size() != wordXpos.size() ||
      words.size() != wordStyles.size() || words.size() > 512 ||
      (paragraphStartWord != UINT16_MAX && paragraphStartWord >= words.size()) ||
      (hasFocus && (words.size() != focusBoundary.size() || words.size() != focusSplitX.size()))) {
    LOG_ERR("TXB", "Construction failed: size mismatch (words=%u, xpos=%u, styles=%u, boundary=%u, splitX=%u)",
            static_cast<uint32_t>(words.size()), static_cast<uint32_t>(wordXpos.size()),
            static_cast<uint32_t>(wordStyles.size()), static_cast<uint32_t>(focusBoundary.size()),
            static_cast<uint32_t>(focusSplitX.size()));
    isValid = false;
    return;
  }

  numWords = static_cast<uint16_t>(words.size());
  focusPresent = hasFocus;
  if (numWords == 0) {
    return;  // valid empty block, no arena
  }

  // Pass 1: total text size, one NUL per word. A line is at most a physical
  // row of the page, so uint16_t offsets are ample; reject anything larger.
  size_t totalText = 0;
  for (const auto& w : words) totalText += w.size() + 1;
  if (totalText > UINT16_MAX) {
    LOG_ERR("TXB", "Construction failed: text size %u exceeds arena limit", static_cast<uint32_t>(totalText));
    numWords = 0;
    focusPresent = false;
    isValid = false;
    return;
  }
  textBytes = static_cast<uint16_t>(totalText);

  const size_t size = arenaSize(numWords, focusPresent, textBytes);
  arena = makeUniqueNoThrow<uint8_t[]>(size);
  if (!arena) {
    // Evict rebuildable caches (SD-font mini data, render glyph cache) and
    // retry once before declaring the line lost.
    freeink::MemoryManager::instance().ensureFree(size + 4 * 1024);
    arena = makeUniqueNoThrow<uint8_t[]>(size);
  }
  if (!arena) {
    LOG_ERR("TXB", "OOM: arena %u bytes", static_cast<uint32_t>(size));
    numWords = 0;
    textBytes = 0;
    focusPresent = false;
    isValid = false;
    return;
  }
  memset(arena.get(), 0, size);
  bindArenaPointers();

  // Pass 2: fill. Mutable aliases of the const views bound above.
  auto* textOff = const_cast<uint16_t*>(textOffArr);
  auto* xpos = const_cast<int16_t*>(xposArr);
  auto* styles = const_cast<uint8_t*>(stylesArr);
  auto* text = const_cast<char*>(textArr);
  uint16_t off = 0;
  for (uint16_t i = 0; i < numWords; i++) {
    const_cast<SourceRange*>(sourceRanges)[i] = ranges.empty() ? SourceRange{} : ranges[i];
    textOff[i] = off;
    xpos[i] = wordXpos[i];
    const_cast<uint8_t*>(spaces)[i] = i > 0;
    styles[i] = static_cast<uint8_t>(wordStyles[i]);
    memcpy(text + off, words[i].data(), words[i].size());
    off += static_cast<uint16_t>(words[i].size());
    text[off++] = '\0';
  }
  if (focusPresent) {
    auto* splitX = const_cast<uint16_t*>(focusSplitXArr);
    auto* boundary = const_cast<uint8_t*>(focusBoundaryArr);
    for (uint16_t i = 0; i < numWords; i++) {
      splitX[i] = focusSplitX[i];
      boundary[i] = focusBoundary[i];
    }
  }
}

bool TextBlock::hasRuby() const {
  for (const auto& rt : rubyTexts) {
    if (!rt.empty()) return true;
  }
  return false;
}

bool TextBlock::setWordFonts(const uint8_t* slots, const int32_t (&fonts)[MAX_WORD_FONTS]) {
  wordFontData = makeUniqueNoThrow<uint8_t[]>(sizeof(fonts) + numWords);
  if (!wordFontData) return false;
  memcpy(wordFontData.get(), fonts, sizeof(fonts));
  memcpy(wordFontData.get() + sizeof(fonts), slots, numWords);
  return true;
}

int TextBlock::renderFontId(const GfxRenderer& renderer, const int fontId, const int sectionFontId) {
  return fontId == sectionFontId || renderer.ensureFontLoaded(fontId) ? fontId : sectionFontId;
}

int TextBlock::wordFontId(const uint16_t i, const int sectionFontId) const {
  if (!wordFontData) return blockFontId(sectionFontId);
  const uint8_t slot = wordFontData[sizeof(int32_t) * MAX_WORD_FONTS + i] & ~SMALL_CAPS_SLOT_FLAG;
  if (slot == 0) return blockFontId(sectionFontId);
  int32_t font = 0;
  memcpy(&font, wordFontData.get() + sizeof(int32_t) * (slot - 1), sizeof(font));
  return font;
}

int TextBlock::lineAscent(const GfxRenderer& renderer, const int sectionFontId) const {
  int ascent = renderer.getFontAscenderSize(renderFontId(renderer, blockFontId(sectionFontId), sectionFontId));
  if (!wordFontData) return ascent;
  for (uint8_t slot = 0; slot < MAX_WORD_FONTS; ++slot) {
    int32_t font = 0;
    memcpy(&font, wordFontData.get() + sizeof(int32_t) * slot, sizeof(font));
    if (font != 0) ascent = std::max(ascent, renderer.getFontAscenderSize(renderFontId(renderer, font, sectionFontId)));
  }
  return ascent;
}

int TextBlock::wordYOffset(const GfxRenderer& renderer, const int sectionFontId, const uint16_t i) const {
  if (!wordFontData) return 0;
  return lineAscent(renderer, sectionFontId) -
         renderer.getFontAscenderSize(renderFontId(renderer, wordFontId(i, sectionFontId), sectionFontId));
}

int TextBlock::extraAscent(const GfxRenderer& renderer, const int sectionFontId) const {
  if (!wordFontData) return 0;
  return lineAscent(renderer, sectionFontId) -
         renderer.getFontAscenderSize(renderFontId(renderer, blockFontId(sectionFontId), sectionFontId));
}

void TextBlock::render(const GfxRenderer& renderer, const int sectionFontId, const int x, const int y,
                       const bool foregroundBlack) const {
  if (!isValid) {
    LOG_ERR("TXB", "Render skipped: invalid block");
    return;
  }
  // A sized variant that can no longer be loaded falls back to the section font.
  const int fontId = renderFontId(renderer, blockFontId(sectionFontId), sectionFontId);
  const int8_t tracking = blockStyle.characterSpacing;

  const bool scanning = renderer.isFontCacheScanning();
  // Words in other sizes share the baseline set by the line's tallest font.
  const int ascender = lineAscent(renderer, sectionFontId);

  // Resolve ruby positions. Layout (extractLine) has already reserved extraStartOffset on the
  // left and extraEndOffset on the right, so the centered rubyX is always within the page margins.
  struct RubyDrawInfo {
    int x = 0;  // spread == 0: centered x. spread > 0: left edge of the base text.
    std::string_view text;
    BidiUtils::BidiBaseDir baseDir{};
    // ruby-align: space-around. >0 when the ruby is narrower than its base: the width difference
    // to distribute over the ruby tokens. 0 means plain centering.
    int spread = 0;
    // Gap before the first and after the last ruby token. Uncapped it is spread / 2n, but it is
    // limited to half a base character; the inner gaps absorb the remainder.
    int edge = 0;
    std::vector<size_t> breaks;  // token boundaries inside `text` (see CjkBreaks)
  };
  // Copies ruby token k of s (tokens are delimited by br) into out.
  const auto rubyToken = [](std::string_view s, const std::vector<size_t>& br, const size_t k, std::string& out) {
    const size_t b = k == 0 ? 0 : br[k - 1];
    const size_t e = k == br.size() ? s.size() : br[k];
    out.assign(s.data() + b, e - b);
  };
  const bool blockHasRuby = hasRuby();
  std::vector<RubyDrawInfo> rubies;
  if (blockHasRuby && !scanning) {
    rubies.resize(numWords);
    for (uint16_t i = 0; i < numWords; i++) {
      if (i < rubyTexts.size() && !rubyTexts[i].empty() && (wordStyle(i) & EpdFontFamily::RUBY_CONTINUE) == 0) {
        int groupWordCount = 1;
        while (i + groupWordCount < numWords && (wordStyle(i + groupWordCount) & EpdFontFamily::RUBY_CONTINUE) != 0) {
          groupWordCount++;
        }
        // Whole-string width: the same measure layout uses to decide whether the ruby overhangs.
        const int rubyWidth =
            renderer.getTextAdvanceXTracked(fontId, rubyTexts[i].c_str(), EpdFontFamily::SUP, tracking);
        const int leaderWordX = xposArr[i] + x;
        const auto baseDir =
            static_cast<BidiUtils::BidiBaseDir>(BidiUtils::detectParagraphLevel(wordText(i), blockStyle.isRtl ? 1 : 0));

        const int lastWordIdx = i + groupWordCount - 1;
        const int lastWordWidth =
            renderer.getTextAdvanceXTracked(fontId, wordText(lastWordIdx), wordStyle(lastWordIdx), tracking);
        const int groupActualWidth = (xposArr[lastWordIdx] + lastWordWidth) - xposArr[i];

        RubyDrawInfo& info = rubies[i];
        info.text = rubyTexts[i];
        info.baseDir = baseDir;
        info.x = leaderWordX - (rubyWidth - groupActualWidth) / 2;  // default: centered

        // CSS Ruby ruby-align: space-around. When the ruby is narrower than its base, the free space is
        // split so that each gap between ruby characters is twice the gap at either end; the centers
        // still coincide. A ruby with a single token (e.g. Latin text) degenerates to centering.
        if (rubyWidth < groupActualWidth) {
          auto breaks = CjkBreaks::characterBreakByteOffsets(rubyTexts[i]);
          if (!breaks.empty()) {
            int tokenSum = 0;
            std::string tok;
            for (size_t k = 0; k <= breaks.size(); ++k) {
              rubyToken(info.text, breaks, k, tok);
              tokenSum += renderer.getTextAdvanceXTracked(fontId, tok.c_str(), EpdFontFamily::SUP, tracking);
            }
            if (tokenSum < groupActualWidth) {
              // Count base characters (UTF-8 lead bytes) to get the average base character width.
              int baseChars = 0;
              for (int k = 0; k < groupWordCount; ++k) {
                for (const char* p = wordText(i + k); *p; ++p) {
                  if ((static_cast<uint8_t>(*p) & 0xC0) != 0x80) baseChars++;
                }
              }
              baseChars = std::max(baseChars, 1);
              // CSS Ruby: the start/end spacing is capped at half the size of one base character.
              const int edgeCap = groupActualWidth / (2 * baseChars);

              const int n = static_cast<int>(breaks.size()) + 1;
              info.x = leaderWordX;
              info.spread = groupActualWidth - tokenSum;
              info.edge = std::min(info.spread / (2 * n), edgeCap);
              info.breaks = std::move(breaks);
            }
          }
        }
        i += groupWordCount - 1;
      }
    }
  }

  struct DecorationLineTracker {
    EpdFontFamily::Style style;
    int startX = -1;
    int endX = -1;
    int yPos = 0;

    bool active() const { return startX != -1; }
    void reset() {
      startX = -1;
      endX = -1;
      yPos = 0;
    }
  };

  DecorationLineTracker decorationLines[] = {
      {EpdFontFamily::UNDERLINE},
      {EpdFontFamily::STRIKETHROUGH},
  };

  const auto flushDecoration = [&](DecorationLineTracker& line) {
    if (line.active()) {
      renderer.drawLine(line.startX, line.yPos, line.endX, line.yPos, 2, foregroundBlack);
      line.reset();
    }
  };
  const auto flushDecorations = [&]() {
    for (auto& line : decorationLines) {
      flushDecoration(line);
    }
  };

  // Loop-invariant: hoisted out of the word loop so rubyTexts is scanned once,
  // not once per word.
  const int rubyShift = getRubyShift(ascender);

  for (uint16_t i = 0; i < numWords; i++) {
    const char* word = wordText(i);
    const int wordX = xposArr[i] + x;
    const EpdFontFamily::Style currentStyle = wordStyle(i);
    const auto baseDir =
        static_cast<BidiUtils::BidiBaseDir>(BidiUtils::detectParagraphLevel(word, blockStyle.isRtl ? 1 : 0));
    const uint8_t boundary = focusBoundary(i);
    const int wordFont = wordFontData ? renderFontId(renderer, wordFontId(i, sectionFontId), sectionFontId) : fontId;
    const int wordAscender = wordFont == fontId && !wordFontData ? ascender : renderer.getFontAscenderSize(wordFont);

    // SUP/SUB shift the baseline passed to drawText; the glyph is also scaled 50% inside
    // drawText, so these offsets are chosen relative to the full-size ascender:
    //   SUP: raise by 40% of ascender — sits clearly above the cap-height
    //   SUB: lower by 25% of ascender — descends below baseline without clashing with ascenders below
    int wordY = y + rubyShift + (ascender - wordAscender);
    if ((currentStyle & EpdFontFamily::SUP) != 0) {
      wordY -= ascender * 2 / 5;
    } else if ((currentStyle & EpdFontFamily::SUB) != 0) {
      wordY += ascender / 4;
    }

    const bool ink = (flags[i] & WORD_FLAG_BACKGROUND_BLACK) ? !foregroundBlack : foregroundBlack;
    if ((flags[i] & WORD_FLAG_BACKGROUND_BLACK) && !scanning) {
      renderer.fillRect(wordX, wordY, renderer.getTextAdvanceXTracked(wordFont, word, currentStyle, tracking),
                        renderer.getLineHeight(wordFont), foregroundBlack);
    }
    if (guideDots[i])
      renderer.drawText(wordFont, wordX + guideDots[i], wordY, "\xc2\xb7", foregroundBlack, EpdFontFamily::REGULAR);
    const int drawX = wordX;

    if (boundary > 0) {
      // Place the logical prefix on the right for RTL words. Each run is reordered by drawText.
      // The bold prefix is bounded to 9 codepoints by the clamp on targetBoldChars in
      // ParsedText::addWord; 9 UTF-8 codepoints occupy at most 9 * 4 = 36 bytes, +1 for null = 37.
      // The split offset is computed at cache-creation time to avoid font metric lookups at render time.
      static constexpr size_t MAX_FOCUS_PREFIX_BYTES = 9 * 4 + 1;
      char boldBuf[40];
      static_assert(sizeof(boldBuf) >= MAX_FOCUS_PREFIX_BYTES,
                    "boldBuf too small for max focus prefix (9 codepoints * 4 UTF-8 bytes + null)");
      const auto boldStyle = static_cast<EpdFontFamily::Style>(currentStyle | EpdFontFamily::BOLD);
      const size_t boldLen =
          std::min<size_t>({static_cast<size_t>(boundary), static_cast<size_t>(wordTextLen(i)), sizeof(boldBuf) - 1});
      memcpy(boldBuf, word, boldLen);
      boldBuf[boldLen] = '\0';
      // Numeric runs keep LTR order even inside an RTL paragraph. Match layout.
      const bool rtl = BidiUtils::startsWithRtl(word);
      const int prefixX = drawX + (rtl ? focusSplitXArr[i] : 0);
      const int suffixX = drawX + (rtl ? 0 : focusSplitXArr[i]);
      renderer.drawText(wordFont, prefixX, wordY, boldBuf, ink, boldStyle, baseDir, tracking);
      renderer.drawText(wordFont, suffixX, wordY, word + boldLen, ink, currentStyle, baseDir, tracking);
    } else {
      renderer.drawText(wordFont, drawX, wordY, word, ink, currentStyle, baseDir, tracking);
    }

    // Horizontal ruby text rendering
    if (blockHasRuby && i < rubyTexts.size() && !rubyTexts[i].empty() &&
        (wordStyle(i) & EpdFontFamily::RUBY_CONTINUE) == 0) {
      if (scanning) {
        renderer.drawText(fontId, 0, 0, rubyTexts[i].c_str(), foregroundBlack, EpdFontFamily::SUP);
      } else {
        const int rubyY = wordY - ascender;
        const RubyDrawInfo& ruby = rubies[i];
        if (ruby.spread == 0) {
          renderer.drawText(fontId, ruby.x, rubyY, rubyTexts[i].c_str(), foregroundBlack, EpdFontFamily::SUP,
                            ruby.baseDir, tracking);
        } else {
          // The edge gap is capped (see layout), so the remaining space is split evenly over the n-1
          // gaps between tokens. Token k starts after the edge gap plus k inner gaps. Computed from k
          // rather than by accumulating a rounded gap, so rounding error does not build up.
          const int n = static_cast<int>(ruby.breaks.size()) + 1;  // n >= 2 here
          const int innerTotal = ruby.spread - 2 * ruby.edge;
          int pen = 0;
          std::string tok;
          for (int k = 0; k < n; ++k) {
            const int tokenIndex = ruby.baseDir == BidiUtils::BidiBaseDir::RTL ? n - 1 - k : k;
            rubyToken(ruby.text, ruby.breaks, static_cast<size_t>(tokenIndex), tok);
            const int gx = ruby.x + pen + ruby.edge + (innerTotal * k + (n - 1) / 2) / (n - 1);
            renderer.drawText(fontId, gx, rubyY, tok.c_str(), foregroundBlack, EpdFontFamily::SUP, ruby.baseDir,
                              tracking);
            pen += renderer.getTextAdvanceXTracked(fontId, tok.c_str(), EpdFontFamily::SUP, tracking);
          }
        }
      }
    }

    if (scanning) {
      continue;
    }

    if (EpdFontFamily::hasTextDecoration(currentStyle)) {
      int lineStartX = drawX;
      int lineWidth = renderer.getTextAdvanceXTracked(wordFont, word, currentStyle, tracking, baseDir,
                                                      GfxRenderer::TextMeasureMode::Rendered);

      // Do not decorate the synthetic em-space used for paragraph indentation.
      if (wordTextLen(i) >= 3 && static_cast<uint8_t>(word[0]) == 0xE2 && static_cast<uint8_t>(word[1]) == 0x80 &&
          static_cast<uint8_t>(word[2]) == 0x83) {
        const char* visibleText = word + 3;
        lineStartX += renderer.getTextAdvanceXTracked(wordFont, "\xe2\x80\x83", currentStyle, tracking, baseDir,
                                                      GfxRenderer::TextMeasureMode::Rendered);
        lineWidth = renderer.getTextAdvanceXTracked(wordFont, visibleText, currentStyle, tracking, baseDir,
                                                    GfxRenderer::TextMeasureMode::Rendered);
      }

      for (auto& line : decorationLines) {
        if ((currentStyle & line.style) == 0) {
          flushDecoration(line);
          continue;
        }

        int offset = 2;
        if (line.style == EpdFontFamily::STRIKETHROUGH) {
          int32_t advance = 0;
          int height = 0;
          const bool smallCaps = (currentStyle & EpdFontFamily::SMALL_CAPS) != 0;
          if (!renderer.getCodepointMetrics(wordFont, smallCaps ? 'X' : 'x', currentStyle, advance, height) ||
              height <= 0) {
            height = wordAscender * 2 / 3;
          }
          if ((currentStyle & (EpdFontFamily::SUP | EpdFontFamily::SUB)) != 0) {
            height = (height + 1) / 2;
          } else if (smallCaps) {
            height = (height * 3 + 3) / 4;
          }
          offset = -std::max(1, height / 2);
        }
        const int lineY = wordY + wordAscender + offset;
        if (line.active() && line.yPos != lineY) {
          flushDecoration(line);
        }
        if (!line.active()) {
          line.startX = lineStartX;
          line.yPos = lineY;
        }
        line.endX = lineStartX + lineWidth;
      }
    } else {
      flushDecorations();
    }
  }
  flushDecorations();
}

bool TextBlock::serialize(HalFile& file) const {
  if (!isValid) {
    LOG_ERR("TXB", "Serialization failed: invalid block");
    return false;
  }

  // Word data: scalars, then the arena verbatim -- its in-memory layout is
  // exactly the on-disk layout (see TextBlock.h), so one write covers all
  // per-word arrays and the text blob.
  if (!serialization::tryWritePod(file, numWords)) return false;
  if (!serialization::tryWritePod(file, static_cast<uint8_t>(focusPresent ? 1 : 0))) return false;
  if (!serialization::tryWritePod(file, textBytes)) return false;
  if (!serialization::tryWritePod(file, paragraphStartWord)) return false;
  if (numWords > 0) {
    const size_t size = arenaSize(numWords, focusPresent, textBytes);
    if (file.write(arena.get(), size) != size) {
      LOG_ERR("TXB", "Serialization failed: arena write (%u bytes)", static_cast<uint32_t>(size));
      return false;
    }
  }

  // Ruby text data
  for (size_t i = 0; i < numWords; i++) {
    if (!serialization::tryWriteString(file, (i < rubyTexts.size()) ? rubyTexts[i] : std::string())) return false;
  }

  // Style (alignment + margins/padding/indent)
  if (!serialization::tryWritePod(file, blockStyle.alignment)) return false;
  if (!serialization::tryWritePod(file, blockStyle.textAlignDefined)) return false;
  if (!serialization::tryWritePod(file, blockStyle.marginTop)) return false;
  if (!serialization::tryWritePod(file, blockStyle.marginBottom)) return false;
  if (!serialization::tryWritePod(file, blockStyle.marginLeft)) return false;
  if (!serialization::tryWritePod(file, blockStyle.marginRight)) return false;
  if (!serialization::tryWritePod(file, blockStyle.paddingTop)) return false;
  if (!serialization::tryWritePod(file, blockStyle.paddingBottom)) return false;
  if (!serialization::tryWritePod(file, blockStyle.paddingLeft)) return false;
  if (!serialization::tryWritePod(file, blockStyle.paddingRight)) return false;
  if (!serialization::tryWritePod(file, blockStyle.textIndent)) return false;
  if (!serialization::tryWritePod(file, blockStyle.textIndentDefined)) return false;
  if (!serialization::tryWritePod(file, blockStyle.isRtl)) return false;
  if (!serialization::tryWritePod(file, blockStyle.directionDefined)) return false;
  if (!serialization::tryWritePod(file, blockStyle.characterSpacing)) return false;
  if (!serialization::tryWritePod(file, blockStyle.fontId)) return false;

  // Per-word fonts: a flag, then the font table and one slot per word.
  const uint8_t hasWordFonts = wordFontData ? 1 : 0;
  if (!serialization::tryWritePod(file, hasWordFonts)) return false;
  if (hasWordFonts) {
    const size_t size = sizeof(int32_t) * MAX_WORD_FONTS + numWords;
    if (file.write(wordFontData.get(), size) != size) {
      LOG_ERR("TXB", "Serialization failed: word fonts");
      return false;
    }
  }

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(HalFile& file) {
  uint16_t wc;
  uint8_t hasFocus;
  uint16_t textBytes;
  uint16_t paragraphStartWord;
  if (!serialization::tryReadPod(file, wc)) return nullptr;
  if (!serialization::tryReadPod(file, hasFocus)) return nullptr;
  if (!serialization::tryReadPod(file, textBytes)) return nullptr;
  if (!serialization::tryReadPod(file, paragraphStartWord)) return nullptr;

  if (hasFocus > 1) return nullptr;
  // Sanity checks: cap the arena allocation and reject impossible geometry
  // (every word carries at least its NUL terminator).
  if (wc > 512 || (paragraphStartWord != UINT16_MAX && paragraphStartWord >= wc)) {
    LOG_ERR("TXB", "Deserialization failed: word count %u exceeds maximum", wc);
    return nullptr;
  }
  if ((wc == 0 && textBytes != 0) || (wc > 0 && textBytes < wc)) {
    LOG_ERR("TXB", "Deserialization failed: bad text size %u for %u words", textBytes, wc);
    return nullptr;
  }

  std::unique_ptr<TextBlock> block(new (std::nothrow) TextBlock());
  if (!block) {
    LOG_ERR("TXB", "OOM: TextBlock");
    return nullptr;
  }
  block->numWords = wc;
  block->textBytes = textBytes;
  block->paragraphStartWord = paragraphStartWord;
  block->focusPresent = hasFocus != 0;

  if (wc > 0) {
    const size_t size = arenaSize(wc, block->focusPresent, textBytes);
    block->arena = makeUniqueNoThrow<uint8_t[]>(size);
    if (!block->arena) {
      LOG_ERR("TXB", "OOM: arena %u bytes", static_cast<uint32_t>(size));
      return nullptr;
    }
    if (file.read(block->arena.get(), size) != size) {
      LOG_ERR("TXB", "Deserialization failed: arena read (%u bytes)", static_cast<uint32_t>(size));
      return nullptr;
    }
    block->bindArenaPointers();

    // Validate offsets before anything dereferences wordText(): offset 0 first,
    // strictly increasing, in bounds, and every word NUL-terminated (word i ends
    // at the byte before offset i+1; the last word at the last text byte).
    const uint16_t* textOff = block->textOffArr;
    const char* text = block->textArr;
    if (textOff[0] != 0 || text[textBytes - 1] != '\0') {
      LOG_ERR("TXB", "Deserialization failed: corrupt text layout");
      return nullptr;
    }
    for (uint16_t i = 1; i < wc; i++) {
      if (textOff[i] <= textOff[i - 1] || textOff[i] >= textBytes || text[textOff[i] - 1] != '\0') {
        LOG_ERR("TXB", "Deserialization failed: corrupt word offset %u", i);
        return nullptr;
      }
    }
  }

  // Ruby text data. Ruby is a CJK feature, so for nearly every book every entry here
  // is the empty string. Materializing the vector regardless costs wordCount * 24 bytes
  // (sizeof(std::string)) plus a heap block per line, held for as long as the page is
  // resident -- several KB of DRAM on a full page, none of it ever read. An empty
  // rubyTexts is already the "no ruby" representation: hasRuby() reports false and every
  // other reader is guarded by `i < rubyTexts.size()`, so allocate lazily and only once a
  // non-empty annotation actually shows up.
  //
  // `scratch` is reused across words: readString() resizes it to the incoming length and
  // overwrites every byte, so a moved-from value carries nothing into the next iteration.
  std::string scratch;
  for (uint16_t i = 0; i < wc; i++) {
    if (!serialization::tryReadString(file, scratch)) return nullptr;
    if (scratch.empty()) continue;
    if (block->rubyTexts.empty()) {
      block->rubyTexts.resize(wc);
    }
    block->rubyTexts[i] = std::move(scratch);
  }

  // Style (alignment + margins/padding/indent)
  BlockStyle& blockStyle = block->blockStyle;
  if (!serialization::tryReadPod(file, blockStyle.alignment)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.textAlignDefined)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.marginTop)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.marginBottom)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.marginLeft)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.marginRight)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.paddingTop)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.paddingBottom)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.paddingLeft)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.paddingRight)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.textIndent)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.textIndentDefined)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.isRtl)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.directionDefined)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.characterSpacing)) return nullptr;
  if (!serialization::tryReadPod(file, blockStyle.fontId)) return nullptr;

  uint8_t hasWordFonts = 0;
  if (!serialization::tryReadPod(file, hasWordFonts)) return nullptr;
  if (hasWordFonts > 1) {
    LOG_ERR("TXB", "Deserialization failed: bad word font flag %u", hasWordFonts);
    return nullptr;
  }
  if (hasWordFonts) {
    const size_t size = sizeof(int32_t) * MAX_WORD_FONTS + wc;
    block->wordFontData = makeUniqueNoThrow<uint8_t[]>(size);
    if (!block->wordFontData || file.read(block->wordFontData.get(), size) != size) {
      LOG_ERR("TXB", "Deserialization failed: word fonts");
      return nullptr;
    }
    const uint8_t* slots = block->wordFontData.get() + sizeof(int32_t) * MAX_WORD_FONTS;
    for (uint16_t i = 0; i < wc; ++i) {
      const uint8_t slot = slots[i] & ~SMALL_CAPS_SLOT_FLAG;
      int32_t font = 1;
      if (slot > 0 && slot <= MAX_WORD_FONTS) {
        memcpy(&font, block->wordFontData.get() + sizeof(int32_t) * (slot - 1), sizeof(font));
      }
      if (slot > MAX_WORD_FONTS || font == 0) {
        LOG_ERR("TXB", "Deserialization failed: bad word font slot %u", slots[i]);
        return nullptr;
      }
    }
  }

  return block;
}

void TextBlock::setWordPresentation(uint16_t i, uint16_t dot, uint8_t flag, bool space) {
  if (i >= numWords) return;
  const_cast<uint16_t*>(guideDots)[i] = dot;
  const_cast<uint8_t*>(flags)[i] = flag;
  const_cast<uint8_t*>(spaces)[i] = space;
}

namespace {
std::vector<EpdFontFamily::Style> legacyBaseStyles(const std::vector<EpdFontFamily::Style>& styles) {
  auto result = styles;
  for (auto& style : result) style = static_cast<EpdFontFamily::Style>(style & ~EpdFontFamily::SMALL_CAPS);
  return result;
}
}  // namespace
TextBlock::TextBlock(const std::vector<std::string>& words, const std::vector<int16_t>& positions,
                     const std::vector<EpdFontFamily::Style>& styles, const std::vector<uint8_t>& boundaries,
                     const std::vector<uint16_t>& splits, const std::vector<uint16_t>& dots,
                     const std::vector<uint8_t>& legacyFlags, const std::vector<bool>& spaces, const BlockStyle& style,
                     std::vector<std::string> ruby)
    : TextBlock(words, positions, legacyBaseStyles(styles), boundaries, splits, style, std::move(ruby)) {
  if (!valid()) return;
  if ((!dots.empty() && dots.size() != words.size()) || (!legacyFlags.empty() && legacyFlags.size() != words.size()) ||
      (!spaces.empty() && spaces.size() != words.size())) {
    isValid = false;
    return;
  }
  bool smallCaps = false;
  for (auto value : styles) smallCaps |= (value & EpdFontFamily::SMALL_CAPS) != 0;
  if (smallCaps) {
    std::vector<uint8_t> slots(numWords);
    for (uint16_t i = 0; i < numWords; ++i) slots[i] = styles[i] & EpdFontFamily::SMALL_CAPS ? SMALL_CAPS_SLOT_FLAG : 0;
    const int32_t fonts[MAX_WORD_FONTS] = {};
    if (!setWordFonts(slots.data(), fonts)) {
      isValid = false;
      return;
    }
  }
  for (uint16_t i = 0; i < numWords; ++i) {
    const uint8_t flag = legacyFlags.empty() ? 0 : legacyFlags[i];
    if (flag & WORD_FLAG_INSERTED_HYPHEN) const_cast<uint8_t*>(stylesArr)[i] |= DISCRETIONARY_HYPHEN_FLAG;
    setWordPresentation(i, dots.empty() ? 0 : dots[i], flag, spaces.empty() ? i > 0 : spaces[i]);
  }
}
