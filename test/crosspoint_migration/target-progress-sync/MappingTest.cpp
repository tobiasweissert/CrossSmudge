#include <gtest/gtest.h>

#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#define private public
#include <Epub.h>
#undef private
#include "ProgressMapper.h"

namespace {
std::shared_ptr<Epub> book() {
  const std::string base = std::string(PROGRESS_FIXTURE_DIR);
  auto epub = std::make_shared<Epub>(base + "/book.epub", base + "/cache");
  epub->cachePath = base + "/cache";
  std::filesystem::create_directories(epub->cachePath);
  epub->bookMetadataCache = std::make_unique<BookMetadataCache>(epub->cachePath);
  auto& cache = *epub->bookMetadataCache;
  EXPECT_TRUE(cache.beginWrite());
  EXPECT_TRUE(cache.beginContentOpfPass());
  cache.createSpineEntry("chapter0.xhtml");
  cache.createSpineEntry("chapter1.xhtml");
  EXPECT_TRUE(cache.endContentOpfPass());
  EXPECT_TRUE(cache.beginTocPass());
  EXPECT_TRUE(cache.endTocPass());
  EXPECT_TRUE(cache.endWrite());
  EXPECT_TRUE(cache.buildBookBin(epub->getPath(), {}));
  EXPECT_TRUE(cache.load());
  return epub;
}
void map(std::shared_ptr<Epub> epub) {
  epub->sourceSpineCount = 1;
  epub->sourceSpineMapCount = 2;
  epub->sourceChildRangeCount = 2;
  epub->sourceSpineMap = std::make_unique<Epub::SourceSpineMapEntry[]>(2);
  epub->sourceChildRanges = std::make_unique<Epub::SourceChildRange[]>(2);
  for (int i = 0; i < 2; ++i) {
    epub->sourceSpineMap[i] = {0, static_cast<uint16_t>(i), 1, 0};
    auto& range = epub->sourceChildRanges[i];
    std::strcpy(range.name, "p");
    range.offset = i * 2;
    range.count = 2;
  }
}
}  // namespace
TEST(ProgressCompatibility, PreciseCurrentAndSourceRoundTrip) {
  auto epub = book();
  map(epub);
  GfxRenderer renderer;
  CrossPointPosition local{1, 0, 5};
  local.visibleTextOffset = 7;
  local.hasVisibleTextOffset = true;
  auto current = ProgressMapper::toKOReader(epub, local);
  ASSERT_TRUE(current.valid);
  EXPECT_EQ(current.xpath, "/body/DocFragment[2]/body/p[2]/text()[1].2");
  auto source = ProgressMapper::toKOReader(epub, local, PositionCoordinateSpace::SourceDocument);
  ASSERT_TRUE(source.valid);
  EXPECT_EQ(source.xpath, "/body/DocFragment[1]/body/p[4]/text()[1].2");
  source.percentage = 0;  // The precise anchor must win over contradictory metadata.
  auto restored = ProgressMapper::toCrossPoint(epub, source, renderer, 1, 5, PositionCoordinateSpace::SourceDocument);
  ASSERT_TRUE(restored.valid);
  EXPECT_EQ(restored.spineIndex, 1);
  EXPECT_TRUE(restored.hasVisibleTextOffset);
  EXPECT_EQ(restored.visibleTextOffset, 7u);
}
TEST(ProgressCompatibility, MissingMapAndUnmappedSourceFailClosed) {
  auto epub = book();
  GfxRenderer renderer;
  epub->sourceSpineMapDeclared = true;
  CrossPointPosition local{0, 0, 5};
  local.hasVisibleTextOffset = true;
  EXPECT_FALSE(ProgressMapper::toKOReader(epub, local, PositionCoordinateSpace::SourceDocument).valid);
  KOReaderPosition incoming{"/body/DocFragment[1]/body/p[4]/text().2", .7f};
  EXPECT_FALSE(
      ProgressMapper::toCrossPoint(epub, incoming, renderer, 0, 5, PositionCoordinateSpace::SourceDocument).valid);
  map(epub);
  incoming.xpath = "/body/DocFragment[1]/body/p[5]/text().2";
  EXPECT_FALSE(
      ProgressMapper::toCrossPoint(epub, incoming, renderer, 0, 5, PositionCoordinateSpace::SourceDocument).valid);
}
TEST(ProgressCompatibility, InvalidPreciseOffsetDoesNotBecomePercentage) {
  auto epub = book();
  CrossPointPosition local{0, 0, 5};
  local.hasVisibleTextOffset = true;
  local.visibleTextOffset = 99999;
  EXPECT_FALSE(ProgressMapper::toKOReader(epub, local).valid);
}
