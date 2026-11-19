#include <gtest/gtest.h>

#include <filesystem>

#include "TouchReaderPreviewModel.h"

static std::unique_ptr<TextBlock> line() {
  auto block = std::make_unique<TextBlock>(
      std::vector<std::string>{"alpha", "beta"}, std::vector<int16_t>{0, 60},
      std::vector<EpdFontFamily::Style>{EpdFontFamily::REGULAR, EpdFontFamily::ITALIC}, std::vector<uint8_t>{2, 0},
      std::vector<uint16_t>{17, 0}, BlockStyle{}, std::vector<std::string>{"ruby", ""},
      std::vector<TextBlock::LinkSpan>{}, std::vector<TextBlock::SourceRange>{{10, 15}, {16, 20}}, 0);
  block->setWordPresentation(0, 45, TextBlock::WORD_FLAG_BACKGROUND_BLACK, false);
  block->setWordPresentation(1, 0, 12, true);
  const uint8_t slots[] = {1, TextBlock::SMALL_CAPS_SLOT_FLAG};
  const int32_t fonts[TextBlock::MAX_WORD_FONTS] = {7, 8, 9};
  EXPECT_TRUE(block->setWordFonts(slots, fonts));
  return block;
}

TEST(ReaderModels, CloneOwnsArenaFontsRubyAndSourceMetadata) {
  auto original = line();
  auto copy = original->clone();
  ASSERT_TRUE(copy);
  EXPECT_NE(copy->wordText(0), original->wordText(0));
  original.reset();
  const auto& block = *copy;
  EXPECT_FALSE(block.isEmpty());
  EXPECT_STREQ(block.wordText(0), "alpha");
  EXPECT_EQ(block.wordSourceRange(1).start, 16U);
  EXPECT_EQ(block.wordSourceRange(1).end, 20U);
  EXPECT_TRUE(block.wordStartsParagraph(0));
  EXPECT_EQ(block.focusRunOffset(0), 17);
  EXPECT_EQ(block.focusBoundary(0), 2);
  EXPECT_EQ(block.guideDotXOffset(0), 45);
  EXPECT_EQ(block.wordFlags(0), TextBlock::WORD_FLAG_BACKGROUND_BLACK);
  EXPECT_EQ(block.wordLinkId(1), 3);
  EXPECT_TRUE(block.wordHasSpaceBefore(1));
  EXPECT_EQ(block.wordFontId(0, 1), 7);
  EXPECT_TRUE(block.wordHasSmallCaps(1));
  EXPECT_EQ(block.getRubyTexts()[0], "ruby");
}

TEST(ReaderModels, PreviewRendersExactSourceAfterPageDestructionAndRecapture) {
  GfxRenderer renderer;
  TouchReaderPreviewModel model;
  {
    Page page;
    page.elements.push_back(std::make_unique<PageLine>(line(), 5, 12));
    ASSERT_TRUE(model.capture(page, renderer, 1, 100, 3, 4));
    model.renderSource(renderer, 1, true);
  }
  const auto expected = renderer.textDraws;
  renderer.textDraws.clear();
  model.renderSource(renderer, 1, true);
  ASSERT_EQ(renderer.textDraws.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(renderer.textDraws[i].text, expected[i].text);
    EXPECT_EQ(renderer.textDraws[i].x, expected[i].x);
    EXPECT_EQ(renderer.textDraws[i].style, expected[i].style);
    EXPECT_EQ(renderer.textDraws[i].direction, expected[i].direction);
  }
  Page empty;
  EXPECT_FALSE(model.capture(empty, renderer, 1, 100));
  renderer.textDraws.clear();
  model.renderSource(renderer, 1, true);
  EXPECT_TRUE(renderer.textDraws.empty());
}

TEST(ReaderModels, ImageBoundsUnionUsesPageCoordinatesAndSkipsEmptyImages) {
  Page page;
  int16_t x = 1, y = 2, w = 3, h = 4;
  EXPECT_FALSE(page.getImageBoundingBox(x, y, w, h));
  page.elements.push_back(std::make_unique<PageLine>(line(), -100, -100));
  page.elements.push_back(std::make_unique<PageImage>(std::make_unique<ImageBlock>("a", "a", 20, 30), -5, 10));
  page.elements.push_back(std::make_unique<PageImage>(std::make_unique<ImageBlock>("b", "b", 40, 10), 10, 5));
  page.elements.push_back(std::make_unique<PageImage>(std::make_unique<ImageBlock>("z", "z", 0, 10), 1000, 1000));
  ASSERT_TRUE(page.getImageBoundingBox(x, y, w, h));
  EXPECT_EQ(x, -5);
  EXPECT_EQ(y, 5);
  EXPECT_EQ(w, 55);
  EXPECT_EQ(h, 35);
  page.elements.pop_back();  // Empty images are not valid persisted elements.
  const auto path = std::filesystem::temp_directory_path() / "target-reader-models-bounds.bin";
  HalFile file;
  ASSERT_TRUE(file.open(path.c_str(), "wb"));
  ASSERT_TRUE(page.serialize(file));
  file.close();
  ASSERT_TRUE(file.open(path.c_str(), "rb"));
  auto warm = Page::deserialize(file);
  file.close();
  std::filesystem::remove(path);
  ASSERT_TRUE(warm);
  ASSERT_TRUE(warm->getImageBoundingBox(x, y, w, h));
  EXPECT_EQ(x, -5);
  EXPECT_EQ(y, 5);
  EXPECT_EQ(w, 55);
  EXPECT_EQ(h, 35);
}
