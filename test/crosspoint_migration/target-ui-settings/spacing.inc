#include "ReaderLineSpacing.h"
TEST(LineSpacing, ModernPercentagesDoNotReuseLegacyCompressionOrdinals) {
  static_assert(CrossPointSettings::TIGHT == 0);
  static_assert(CrossPointSettings::NORMAL == 1);
  static_assert(CrossPointSettings::WIDE == 2);
  static_assert(CrossPointSettings::LINE_COMPRESSION_COUNT == 3);
  EXPECT_EQ(std::size(LINE_SPACING_IDS), 7u);
  const uint8_t percentages[] = {100, 110, 120, 130, 145, 160, 175};
  for (uint8_t i = 0; i < 7; ++i) {
    EXPECT_EQ(lineSpacingPercentForIndex(i), percentages[i]);
    EXPECT_EQ(lineSpacingIndexForPercent(percentages[i]), i);
    EXPECT_FLOAT_EQ(LINE_SPACING_MULTIPLIERS[i], percentages[i] / 100.f);
  }
  EXPECT_EQ(lineSpacingPercentForIndex(255), 120);
  EXPECT_EQ(lineSpacingIndexForPercent(123), -1);
}
