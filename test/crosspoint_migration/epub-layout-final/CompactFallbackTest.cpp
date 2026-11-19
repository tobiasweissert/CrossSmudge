#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <GfxRenderer.h>
#include <MemoryBudget.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <new>

// Exercise production makeUniqueNoThrow paths; ordinary C++ allocations still use
// the real allocator (including sanitizer instrumentation).
namespace {
size_t failAllocationSize = 0;
bool allocationFailed = false;
}  // namespace
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  if (size == failAllocationSize) {
    failAllocationSize = 0;
    allocationFailed = true;
    return nullptr;
  }
  try {
    return ::operator new(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  if (size == failAllocationSize) {
    failAllocationSize = 0;
    allocationFailed = true;
    return nullptr;
  }
  try {
    return ::operator new[](size);
  } catch (...) {
    return nullptr;
  }
}
namespace {
struct CompactFixture {
  GfxRenderer renderer;
  Epub book;
  std::vector<std::unique_ptr<Page>> pages;
  std::vector<uint32_t> references;
  bool compact = false, oom = false;
  std::string path = "/root/.hermes/cache/scratch/epub-layout-final-r2/compact.xhtml";
  CompactFixture() { MemoryBudget::currentHeap = {80 * 1024, 32 * 1024}; }
  ~CompactFixture() {
    MemoryBudget::currentHeap = {};
    failAllocationSize = 0;
    std::filesystem::remove(path);
  }
  bool parse(std::string body, int width = 480, int height = 200) {
    std::ofstream(path) << "<html><body>" << body << "</body></html>";
    ChapterHtmlSlimParser parser(
        book, path, renderer, 0, 1, false, false, 0, width, height, false, false, false, 0,
        [&](std::unique_ptr<Page> page, auto, auto, auto, uint32_t ref) {
          pages.push_back(std::move(page));
          references.push_back(ref);
        },
        true, "", "/root/.hermes/cache/scratch/epub-layout-final-r2/image_", 0, {}, nullptr, nullptr,
        EpubRenderMode::CrossSmudgeDefault, "", 0, true);
    parser.setParagraphIndentSpaces(0);
    const bool result = parser.parseAndBuildPages();
    compact = parser.usedCompactTableFallback();
    oom = parser.layoutWasAbortedForLowMemory();
    return result;
  }
};
std::vector<PageTextLine> lines(const Page& page) {
  std::vector<PageTextLine> out;
  page.forEachTextLine(
      [](const PageTextLine& line, void* ctx) {
        static_cast<std::vector<PageTextLine>*>(ctx)->push_back(line);
        return true;
      },
      &out);
  return out;
}
std::string rowText(int count) {
  std::string out;
  for (int i = 0; i < count; ++i) out += "abcdefghijk ";
  return out;
}
std::unique_ptr<Page> warm(const Page& page) {
  const std::string path = "/root/.hermes/cache/scratch/epub-layout-final-r2/compact-page.bin";
  HalFile out;
  if (!Storage.openFileForWrite("test", path, out) || !page.serialize(out)) return {};
  out.close();
  HalFile in;
  if (!Storage.openFileForRead("test", path, in)) return {};
  auto loaded = Page::deserialize(in);
  in.close();
  std::filesystem::remove(path);
  return loaded;
}
}  // namespace
TEST(CompactFallback, LowHeapUsesActualCompactGridBeyondRichTokenLimit) {
  CompactFixture f;
  ASSERT_TRUE(f.parse("<table><tr><td>" + rowText(100) + "</td><td>" + rowText(100) + "</td></tr></table><p>After</p>",
                      480, 2000));
  ASSERT_TRUE(f.compact);
  ASSERT_FALSE(f.oom);
  size_t words = 0;
  bool after = false;
  for (const auto& page : f.pages)
    for (const auto& line : lines(*page))
      for (uint16_t i = 0; i < line.block->wordCount(); ++i) {
        if (std::string(line.block->wordText(i)) == "After")
          after = true;
        else {
          ++words;
          EXPECT_TRUE(line.isTableText);
          EXPECT_NE(line.tableSelection, UINT16_MAX);
          EXPECT_GT(line.clipWidth, 0);
        }
        EXPECT_GT(line.block->wordSourceRange(i).end, line.block->wordSourceRange(i).start);
      }
  EXPECT_EQ(words, 200u);
  EXPECT_TRUE(after);
  RecordProperty("compact_workspace_bytes", static_cast<int>(sizeof(CompactTableLayout)));
}
TEST(CompactFallback, SourceReferenceSelectionAndFootnotesSurviveWarmPages) {
  CompactFixture f;
  ASSERT_TRUE(
      f.parse("<table><tr><td><b>Alpha</b></td><td><a "
              "href='#note'>Beta</a></td></tr><tr><td>Gamma</td><td>Delta</td></tr><tr><td>Epsilon</td><td>Zeta</td></"
              "tr></table>",
              240, 50));
  ASSERT_TRUE(f.compact);
  ASSERT_GE(f.pages.size(), 3u);
  const std::vector<std::string> expected = {"Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta"};
  size_t index = 0;
  uint32_t offset = 0;
  uint16_t firstColumn = UINT16_MAX;
  for (size_t p = 0; p < f.pages.size(); ++p) {
    auto loaded = warm(*f.pages[p]);
    ASSERT_TRUE(loaded);
    auto cold = lines(*f.pages[p]), cached = lines(*loaded);
    ASSERT_EQ(cold.size(), cached.size());
    if (!cold.empty()) EXPECT_EQ(f.references[p], cold.front().block->wordSourceRange(0).start);
    for (size_t j = 0; j < cold.size(); ++j) {
      const auto& line = cold[j];
      EXPECT_TRUE(line.isTableText);
      ASSERT_LT(index, expected.size());
      ASSERT_EQ(line.block->wordCount(), 1);
      EXPECT_EQ(line.block->wordText(0), expected[index]);
      EXPECT_EQ(line.block->wordSourceRange(0).start, offset);
      offset += expected[index].size();
      EXPECT_EQ(line.block->wordSourceRange(0).end, offset);
      EXPECT_EQ(cached[j].block->wordSourceRange(0).start, line.block->wordSourceRange(0).start);
      EXPECT_EQ(cached[j].tableSelection, line.tableSelection);
      EXPECT_EQ(cached[j].clipWidth, line.clipWidth);
      if (index == 0) {
        firstColumn = line.tableSelection;
        EXPECT_TRUE(line.block->wordStyle(0) & EpdFontFamily::BOLD);
      }
      if (index % 2 == 0) EXPECT_EQ(line.tableSelection, firstColumn);
      if (index == 1) {
        ASSERT_EQ(loaded->footnotes.size(), 1u);
        EXPECT_EQ(line.block->wordLinkId(0), loaded->footnotes[0].linkId);
        EXPECT_STREQ(loaded->footnotes[0].href, "#note");
        ASSERT_FALSE(loaded->links.empty());
      }
      ++index;
    }
  }
  EXPECT_EQ(index, expected.size());
}
TEST(CompactFallback, TallRowsFlattenWithoutLosingSource) {
  CompactFixture f;
  ASSERT_TRUE(f.parse("<table><tr><td>" + rowText(30) + "</td><td>" + rowText(30) + "</td></tr></table>", 240, 64));
  ASSERT_TRUE(f.compact);
  EXPECT_GT(f.pages.size(), 1u);
  size_t count = 0;
  uint32_t prior = 0;
  for (const auto& page : f.pages)
    for (const auto& line : lines(*page))
      for (uint16_t i = 0; i < line.block->wordCount(); ++i) {
        EXPECT_FALSE(line.isTableText);
        const auto range = line.block->wordSourceRange(i);
        EXPECT_GE(range.start, prior);
        prior = range.end;
        ++count;
      }
  EXPECT_EQ(count, 60u);
}
TEST(CompactFallback, AllocationFailurePoisonsBuildRatherThanPublishingSuccess) {
  for (size_t size : {sizeof(CompactTableLayout), size_t(4096)}) {
    CompactFixture f;
    allocationFailed = false;
    failAllocationSize = size;
    const bool ok =
        f.parse("<table><tr><td>" + rowText(100) + "</td><td>" + rowText(100) + "</td></tr></table>", 480, 2000);
    EXPECT_TRUE(allocationFailed);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(f.oom);
  }
}
TEST(CompactFallback, TokenLimitRejectsIncompleteRows) {
  CompactFixture f;
  EXPECT_FALSE(
      f.parse("<table><tr><td>" + rowText(130) + "</td><td>" + rowText(130) + "</td></tr></table>", 480, 2000));
  EXPECT_TRUE(f.oom);
}
TEST(CompactFallback, AmpleHeapKeepsModernRichPath) {
  CompactFixture f;
  MemoryBudget::currentHeap = {};
  ASSERT_TRUE(f.parse("<table><tr><td>Alpha</td><td>Beta</td></tr></table>"));
  EXPECT_FALSE(f.compact);
}
TEST(CompactFallback, ColspansAndUnicodeSplitsRetainRealSourceRanges) {
  CompactFixture f;
  ASSERT_TRUE(
      f.parse("<table><tr><th colspan='2'>Heading</th></tr><tr><td>éééééééééééééééééééé</td><td>Beta</td></tr></table>",
              120, 200));
  EXPECT_TRUE(f.compact);
  uint32_t offset = 0;
  std::string text;
  std::map<uint32_t, std::pair<uint32_t, std::string>> source;
  for (const auto& page : f.pages)
    for (const auto& line : lines(*page))
      for (uint16_t i = 0; i < line.block->wordCount(); ++i) {
        const auto range = line.block->wordSourceRange(i);
        EXPECT_TRUE(source.emplace(range.start, std::make_pair(range.end, line.block->wordText(i))).second);
        if (std::string(line.block->wordText(i)) == "Heading") EXPECT_EQ(line.tableSelection, UINT16_MAX);
      }
  for (const auto& [start, word] : source) {
    EXPECT_EQ(start, offset);
    offset = word.first;
    text += word.second;
  }
  EXPECT_EQ(text, "HeadingééééééééééééééééééééBeta");
  EXPECT_EQ(offset, 31u);
}
TEST(CompactFallback, NestedAndBlockCellsFlattenWithoutDroppingText) {
  CompactFixture f;
  ASSERT_TRUE(
      f.parse("<table><tr><td>Alpha<table><tr><td>Nested</td></tr></table>Tail</td><td><p>Beta</p></td></tr></"
              "table><p>After</p>"));
  EXPECT_TRUE(f.compact);
  std::string text;
  for (const auto& page : f.pages)
    for (const auto& line : lines(*page))
      for (uint16_t i = 0; i < line.block->wordCount(); ++i) text += line.block->wordText(i);
  EXPECT_EQ(text, "AlphaNestedTailBetaAfter");
}
TEST(CompactFallback, TableInsetsUseTheCapturedGeometry) {
  CompactFixture f;
  ASSERT_TRUE(f.parse("<table style='margin-left:12px;margin-right:12px'><tr><td>Alpha</td><td>Beta</td></tr></table>",
                      240, 200));
  ASSERT_FALSE(f.pages.empty());
  const auto text = lines(*f.pages[0]);
  ASSERT_EQ(text.size(), 2u);
  EXPECT_EQ(text[0].xPos, 16);
  EXPECT_EQ(text[0].clipWidth, 100);
  EXPECT_EQ(text[1].xPos, 124);
}

TEST(CompactFallback, RtlMirrorsCellsWithoutChangingLogicalSelection) {
  std::vector<uint16_t> richSelections;
  for (const bool lowHeap : {false, true}) {
    CompactFixture f;
    if (!lowHeap) MemoryBudget::currentHeap = {};
    ASSERT_TRUE(f.parse("<table><tr dir='rtl'><td>Alpha</td><td>Beta</td></tr></table>", 240, 200));
    EXPECT_EQ(f.compact, lowHeap);
    ASSERT_FALSE(f.pages.empty());
    const auto text = lines(*f.pages[0]);
    ASSERT_EQ(text.size(), 2u);
    EXPECT_STREQ(text[0].block->wordText(0), "Alpha");
    EXPECT_GT(text[0].xPos, text[1].xPos);
    for (size_t i = 0; i < text.size(); ++i) {
      EXPECT_TRUE(text[i].isTableText);
      if (lowHeap)
        EXPECT_EQ(text[i].tableSelection, richSelections[i]);
      else
        richSelections.push_back(text[i].tableSelection);
    }
  }
  CompactFixture f;
  ASSERT_TRUE(f.parse("<table><tr dir='rtl'><td colspan='2'>Span</td><td>Tail</td></tr></table>", 240, 200));
  const auto text = lines(*f.pages[0]);
  ASSERT_EQ(text.size(), 2u);
  EXPECT_EQ(text[0].xPos, 84);
  EXPECT_EQ(text[1].xPos, 4);
  EXPECT_EQ(text[0].tableSelection, UINT16_MAX);
  EXPECT_NE(text[1].tableSelection, UINT16_MAX);
}
