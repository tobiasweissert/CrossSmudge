#include <ClippingStore.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstring>

TEST(ClippingUtf8, CompleteCodepointsAtEveryTitleBoundarySurviveReload) {
  for (const std::string codepoint : {std::string("é"), std::string("가"), std::string("😀")}) {
    // Include every cut inside the codepoint, exact fit, and no remaining
    // payload space. Exercise both APIs.
    for (size_t remaining = 0; remaining <= codepoint.size(); ++remaining) {
      for (const bool modern : {false, true}) {
        SCOPED_TRACE(::testing::Message() << codepoint << " remaining=" << remaining << " modern=" << modern);
        fake::reset();
        ClippingStore store;
        ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
        const std::string prefix(CLIPPING_CHAPTER_TITLE_MAX - 1 - remaining, 'x');
        const std::string title = prefix + codepoint + (remaining == codepoint.size() ? "" : codepoint);
        const std::string expected = prefix + (remaining == codepoint.size() ? codepoint : "");
        const auto result =
            modern ? store.addClipping(2, 3, 4, 8, 5, 6, 9, title.c_str(), 7, "body", uint32_t{11}, 12, 20)
                   : store.addClipping(2, 3, 4, 8, 5, 6, 9, title.c_str(), 7, "body", uint16_t{13}, uint32_t{11});
        ASSERT_EQ(result, ClippingStore::AddResult::Added);
        for (int reload = 0; reload < 2; ++reload) {
          ASSERT_EQ(store.clippingCount(), 1u);
          const Clipping* record = store.clippingAt(0);
          ASSERT_NE(record, nullptr);
          ASSERT_NE(memchr(record->chapterTitle, 0, sizeof(record->chapterTitle)), nullptr);
          EXPECT_EQ(std::string(record->chapterTitle), expected);
          EXPECT_LE(strlen(record->chapterTitle), CLIPPING_CHAPTER_TITLE_MAX - 1);
          EXPECT_EQ(record->spineIndex, 2);
          EXPECT_EQ(record->startPage, 3);
          EXPECT_EQ(record->endPage, 4);
          EXPECT_EQ(record->startWordIndex, 5);
          EXPECT_EQ(record->endWordIndex, 6);
          EXPECT_EQ(record->layoutSignature, 11u);
          EXPECT_EQ(record->tableSelection, modern ? UINT16_MAX : 13);
          EXPECT_EQ(record->startOffset, modern ? 12u : UINT32_MAX);
          EXPECT_EQ(record->endOffset, modern ? 20u : UINT32_MAX);
          std::string text;
          ASSERT_TRUE(store.readClippingText(0, text));
          EXPECT_EQ(text, "body");
          store.unload();
          ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
        }
      }
    }
  }
}
