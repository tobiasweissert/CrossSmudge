#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/converters/ImageDimsProbe.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
// Same private-inspection technique as the inherited parser regressions.
#define class struct
#define private public
#include "Epub/parsers/ChapterHtmlSlimParser.h"
#undef private
#undef class

TEST(ReaderConstructors, BothParserConstructorsBindSuppliedRenderer) {
  GfxRenderer renderer;
  ChapterHtmlSlimParser modern(nullptr, "unused", renderer, 0, 1.0f, false, 0, 480, 800, false, false, {}, true, "",
                               "");
  EXPECT_EQ(&modern.renderer, &renderer);
  Epub book;
  ChapterHtmlSlimParser owner(book, "unused", renderer, 0, 1.0f, false, false, 0, 480, 800, false, false, false, 0, {},
                              true, "", "");
  EXPECT_EQ(&owner.renderer, &renderer);
}
