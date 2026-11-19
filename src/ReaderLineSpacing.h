#pragma once

#include <I18n.h>

#include <iterator>

#include "CrossPointSettings.h"

// Labels and multipliers follow MODERN_LINE_COMPRESSION, not the legacy
// TIGHT/NORMAL/WIDE migration field. Persist new choices as lineHeightPercent.
inline constexpr StrId LINE_SPACING_IDS[] = {
    StrId::STR_SPACING_0_PERCENT,       StrId::STR_SPACING_PLUS_10_PERCENT, StrId::STR_SPACING_PLUS_20_PERCENT_DEFAULT,
    StrId::STR_SPACING_PLUS_30_PERCENT, StrId::STR_SPACING_PLUS_45_PERCENT, StrId::STR_SPACING_PLUS_60_PERCENT,
    StrId::STR_SPACING_PLUS_75_PERCENT};
inline constexpr float LINE_SPACING_MULTIPLIERS[] = {1.0f, 1.1f, 1.2f, 1.3f, 1.45f, 1.6f, 1.75f};
inline constexpr uint8_t LINE_SPACING_PERCENTS[] = {100, 110, 120, 130, 145, 160, 175};
inline constexpr uint8_t LINE_SPACING_RAW_VALUES[] = {
    CrossPointSettings::LINE_SPACING_100, CrossPointSettings::LINE_SPACING_110, CrossPointSettings::LINE_SPACING_120,
    CrossPointSettings::LINE_SPACING_130, CrossPointSettings::LINE_SPACING_145, CrossPointSettings::LINE_SPACING_160,
    CrossPointSettings::LINE_SPACING_175};
static_assert(std::size(LINE_SPACING_IDS) == CrossPointSettings::LINE_SPACING_175 + 1);
static_assert(std::size(LINE_SPACING_MULTIPLIERS) == CrossPointSettings::LINE_SPACING_175 + 1);
static_assert(std::size(LINE_SPACING_PERCENTS) == std::size(LINE_SPACING_IDS));
static_assert([] {
  for (size_t i = 0; i < std::size(LINE_SPACING_RAW_VALUES); ++i) {
    if (LINE_SPACING_RAW_VALUES[i] != i) return false;
  }
  return true;
}());

inline constexpr uint8_t lineSpacingPercentForIndex(const int index) {
  return index >= 0 && static_cast<size_t>(index) < std::size(LINE_SPACING_PERCENTS)
             ? LINE_SPACING_PERCENTS[index]
             : LINE_SPACING_PERCENTS[CrossPointSettings::LINE_SPACING_120];
}

// An arbitrary owner percentage is not a legacy ordinal and must not be
// rounded/re-saved merely because this seven-choice picker was opened.
inline constexpr int lineSpacingIndexForPercent(const uint8_t percent) {
  for (size_t i = 0; i < std::size(LINE_SPACING_PERCENTS); ++i) {
    if (LINE_SPACING_PERCENTS[i] == percent) return static_cast<int>(i);
  }
  return -1;
}
