#include <HalStorage.h>
#include <SdCardFontRegistry.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <new>
#include <string>

// Allocation failure now surfaces as an incomplete directory enumeration,
// not a removed compact-filename new[] allocation.

namespace {

std::string pathOf(const SdCardFontFamilyInfo& family, const SdCardFontFileInfo& file) {
  char out[SdCardFontFamilyInfo::PATH_BUFFER_SIZE];
  EXPECT_TRUE(family.filePath(file, out, sizeof(out)));
  return out;
}

class SdFontRegistryTest : public ::testing::Test {
 protected:
  void SetUp() override { fake::reset(); }
  void TearDown() override { fake::reset(); }
  SdCardFontRegistry registry;
};

TEST_F(SdFontRegistryTest, CanonicalNamesHydrateExactPaths) {
  fake::add("/.fonts/Bitter/Bitter_12.cpfont");
  fake::add("/.fonts/Bitter/Bitter_14.cpfont");

  ASSERT_TRUE(registry.discover());
  const auto* family = registry.findFamily("Bitter");
  ASSERT_NE(family, nullptr);
  EXPECT_STREQ(family->root, SdCardFontRegistry::FONTS_DIR_HIDDEN);
  ASSERT_TRUE(family->ensureDetails());
  ASSERT_EQ(family->files.size(), 2u);
  for (const auto& file : family->files) EXPECT_EQ(file.path, pathOf(*family, file));

  const auto* file14 = family->findFile(14);
  ASSERT_NE(file14, nullptr);
  EXPECT_EQ(pathOf(*family, *file14), "/.fonts/Bitter/Bitter_14.cpfont");
  EXPECT_EQ(pathOf(*family, *family->findFile(12)), "/.fonts/Bitter/Bitter_12.cpfont");
}

TEST_F(SdFontRegistryTest, OtherStemsAndZeroPaddedSizesKeepTheirFilename) {
  fake::add("/fonts/Bookerly/Bookerly-SD_14.cpfont");
  fake::add("/fonts/Bookerly/Bookerly_016.cpfont");

  ASSERT_TRUE(registry.discover());
  const auto* family = registry.findFamily("Bookerly");
  ASSERT_NE(family, nullptr);
  EXPECT_STREQ(family->root, SdCardFontRegistry::FONTS_DIR_VISIBLE);

  const auto* file14 = family->findFile(14);
  ASSERT_NE(file14, nullptr);
  EXPECT_EQ(file14->path, "/fonts/Bookerly/Bookerly-SD_14.cpfont");
  EXPECT_EQ(pathOf(*family, *file14), "/fonts/Bookerly/Bookerly-SD_14.cpfont");

  const auto* file16 = family->findFile(16);
  ASSERT_NE(file16, nullptr);
  EXPECT_EQ(file16->path, "/fonts/Bookerly/Bookerly_016.cpfont");
  EXPECT_EQ(pathOf(*family, *file16), "/fonts/Bookerly/Bookerly_016.cpfont");
}

TEST_F(SdFontRegistryTest, HiddenRootWinsAndKeepsItsOwnRoot) {
  fake::add("/.fonts/Lora/Lora_14.cpfont");
  fake::add("/fonts/Lora/Lora_18.cpfont");
  fake::add("/fonts/Inter/Inter_14.cpfont");

  ASSERT_TRUE(registry.discover());
  ASSERT_EQ(registry.getFamilyCount(), 2);
  const auto* lora = registry.findFamily("Lora");
  ASSERT_NE(lora, nullptr);
  EXPECT_STREQ(lora->root, SdCardFontRegistry::FONTS_DIR_HIDDEN);
  EXPECT_FALSE(lora->hasSize(18));
  const auto* inter = registry.findFamily("Inter");
  ASSERT_NE(inter, nullptr);
  EXPECT_EQ(pathOf(*inter, inter->files[0]), "/fonts/Inter/Inter_14.cpfont");
}

TEST_F(SdFontRegistryTest, IncompleteDirectoryAllocationRefusesPartialIndexAndRetries) {
  fake::add("/.fonts/Bookerly/Bookerly-SD_14.cpfont");
  fake::add("/.fonts/Bookerly/Bookerly_16.cpfont");
  fake::failNextDirectoryEntry = true;
  EXPECT_FALSE(registry.discover());
  EXPECT_FALSE(fake::failNextDirectoryEntry);
  EXPECT_TRUE(registry.lastDiscoveryFailed());
  EXPECT_EQ(registry.getFamilyCount(), 0);
  ASSERT_TRUE(registry.discover());
  EXPECT_FALSE(registry.lastDiscoveryFailed());
  const auto* family = registry.findFamily("Bookerly");
  ASSERT_NE(family, nullptr);
  ASSERT_NE(family->findFile(14), nullptr);
  ASSERT_NE(family->findFile(16), nullptr);
  EXPECT_EQ(pathOf(*family, *family->findFile(14)), "/.fonts/Bookerly/Bookerly-SD_14.cpfont");
  EXPECT_EQ(pathOf(*family, *family->findFile(16)), "/.fonts/Bookerly/Bookerly_16.cpfont");
}

TEST_F(SdFontRegistryTest, OverlongPathsAreSkippedAtDiscovery) {
  // "/.fonts/" + 100 + "/" + 100 + "_14.cpfont" = 219 bytes: over PATH_BUFFER_SIZE.
  const std::string longName(100, 'L');
  fake::add("/.fonts/" + longName + "/" + longName + "_14.cpfont");
  fake::add("/.fonts/" + longName + "/Short_16.cpfont");  // 124 bytes: fits
  // The 120-byte filename fits the 128-byte name buffer, but the 239-byte path
  // does not fit PATH_BUFFER_SIZE.
  const std::string onlyLong(110, 'O');
  fake::add("/.fonts/" + onlyLong + "/" + onlyLong + "_12.cpfont");

  ASSERT_TRUE(registry.discover());
  EXPECT_EQ(registry.findFamily(onlyLong), nullptr);  // no loadable file left
  const auto* family = registry.findFamily(longName);
  ASSERT_NE(family, nullptr);
  ASSERT_TRUE(family->ensureDetails());
  ASSERT_EQ(family->files.size(), 1u);
  EXPECT_EQ(family->findFile(14), nullptr);
  const auto* nearest = family->findNearestSize(14);
  ASSERT_NE(nearest, nullptr);
  EXPECT_EQ(nearest->pointSize, 16);
  EXPECT_EQ(pathOf(*family, *nearest), "/.fonts/" + longName + "/Short_16.cpfont");
}

TEST_F(SdFontRegistryTest, FilePathReportsTruncation) {
  fake::add("/.fonts/Bitter/Bitter_14.cpfont");
  ASSERT_TRUE(registry.discover());
  const auto* family = registry.findFamily("Bitter");
  ASSERT_NE(family, nullptr);

  ASSERT_TRUE(family->ensureDetails());
  char tiny[16];
  EXPECT_FALSE(family->filePath(family->files[0], tiny, sizeof(tiny)));
  EXPECT_EQ(tiny[sizeof(tiny) - 1], '\0');
}

}  // namespace
