#include <Arduino.h>
#include <CssParser.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <new>

// Observe the real fallible array boundary. Delegate allocation/deallocation to
// the C++ runtime so ASan still sees matching new[]/delete[] operations.
namespace {
bool failCssArrays = false;
size_t arrayAttempts = 0, failArrayAttempt = 0, liveArrayBytes = 0, liveArrayCount = 0;
struct ArrayAllocation {
  void* pointer = nullptr;
  size_t bytes = 0;
};
ArrayAllocation arrayAllocations[64];
void forgetArray(void* pointer) {
  for (auto& allocation : arrayAllocations) {
    if (allocation.pointer && allocation.pointer == pointer) {
      liveArrayBytes -= allocation.bytes;
      --liveArrayCount;
      allocation = {};
      return;
    }
  }
}
}  // namespace
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  ++arrayAttempts;
  if (failCssArrays || arrayAttempts == failArrayAttempt) return nullptr;
  try {
    void* pointer = ::operator new[](size);
    for (auto& allocation : arrayAllocations) {
      if (!allocation.pointer) {
        allocation = {pointer, size};
        liveArrayBytes += size;
        ++liveArrayCount;
        return pointer;
      }
    }
    std::abort();  // A fixture tracking overflow must never silently lose coverage.
  } catch (...) {
    return nullptr;
  }
}
extern "C" void __real__ZdaPv(void*);
extern "C" void __real__ZdaPvm(void*, size_t);
extern "C" void __wrap__ZdaPv(void* pointer) {
  forgetArray(pointer);
  __real__ZdaPv(pointer);
}
extern "C" void __wrap__ZdaPvm(void* pointer, size_t size) {
  forgetArray(pointer);
  __real__ZdaPvm(pointer, size);
}

struct CssArenaBackingTest : testing::Test {
  void SetUp() override {
    fakeheap::reset();
    Storage.reset();
    arrayAttempts = failArrayAttempt = 0;
    ASSERT_EQ(liveArrayCount, 0u);
  }
  void TearDown() override {
    failCssArrays = false;
    failArrayAttempt = 0;
    EXPECT_EQ(liveArrayCount, 0u);
    EXPECT_EQ(liveArrayBytes, 0u);
    EXPECT_TRUE(fakeheap::live.empty());
    Storage.reset();
  }
  void createCache() {
    const std::string text =
        "p { text-align: center; margin-top: 12px; } .em { font-weight: bold; } div p { font-style: italic; } "
        ".plain { list-style-type: none; }";
    Storage.put("input.css", {text.begin(), text.end()});
    FsFile file;
    ASSERT_TRUE(Storage.openFileForRead("test", "input.css", file));
    CssParser css("book");
    ASSERT_EQ(css.loadFromStream(file), CssParser::ParseResult::Complete);
    file.close();
    ASSERT_TRUE(css.saveToCache(true));
  }
  void checkStyle(CssParser& css) {
    EXPECT_FALSE(css.empty());
    // Four distinct supported selectors, including the formerly ignored
    // contextual `div p`; this is not a count of declaration blocks.
    EXPECT_EQ(css.ruleCount(), 4u);
    EXPECT_EQ(css.resolveStyle("p", "").textAlign, CssTextAlign::Center);
    EXPECT_FALSE(css.resolveStyle("p", "").hasFontStyle());
    EXPECT_EQ(css.resolveStyle("span", "em").fontWeight, CssFontWeight::Bold);
    const auto ancestor = CssParser::makeAncestor("div", "", "");
    const auto style = css.resolveStyle("p", "em", "", &ancestor, 1);
    EXPECT_TRUE(style.hasFontWeight());
    EXPECT_TRUE(style.hasFontStyle());
    EXPECT_TRUE(style.hasTextAlign());
    EXPECT_TRUE(style.hasMarginTop());
    EXPECT_EQ(style.marginTop.value, 12);
    EXPECT_EQ(style.fontWeight, CssFontWeight::Bold);
    EXPECT_EQ(style.fontStyle, CssFontStyle::Italic);
    EXPECT_EQ(style.textAlign, CssTextAlign::Center);
    const auto listStyle = css.resolveStyle("ol", "plain");
    EXPECT_TRUE(listStyle.hasListStyleType());
    EXPECT_EQ(listStyle.listStyleType, CssListStyleType::None);
  }
};
TEST_F(CssArenaBackingTest, IdenticalStylesAcrossCapabilityHeapAvailability) {
  size_t hydratedBytes = 0;
  for (int mode = 0; mode < 3; ++mode) {
    fakeheap::reset(mode != 0);
    createCache();
    ASSERT_EQ(liveArrayCount, 0u);
    CssParser css("book");
    if (mode == 2) fakeheap::external.fail = 1;
    ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
    checkStyle(css);
    // Exactly the three bounded pools remain; cache scratch was released.
    EXPECT_EQ(liveArrayCount, 3u);
    EXPECT_GT(liveArrayBytes, 0u);
    if (mode == 0) hydratedBytes = liveArrayBytes;
    EXPECT_EQ(liveArrayBytes, hydratedBytes);
    EXPECT_EQ(fakeheap::internal.attempts, 0u);
    EXPECT_EQ(fakeheap::external.attempts, 0u);
    EXPECT_TRUE(fakeheap::live.empty());
    css.clear();
    EXPECT_TRUE(css.empty());
    EXPECT_EQ(css.ruleCount(), 0u);
    EXPECT_EQ(liveArrayCount, 0u);
    EXPECT_EQ(liveArrayBytes, 0u);
  }
}
TEST_F(CssArenaBackingTest, FlatHydrationDoesNotConsumeCapabilityReserves) {
  createCache();
  for (bool internalPressure : {false, true}) {
    fakeheap::reset();
    CssParser css("book");
    if (internalPressure)
      fakeheap::internal.free = 79 * 1024;
    else
      fakeheap::external.free = 128 * 1024;
    const auto internalFree = fakeheap::internal.free;
    const auto externalFree = fakeheap::external.free;
    ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
    checkStyle(css);
    EXPECT_EQ(liveArrayCount, 3u);
    EXPECT_EQ(fakeheap::internal.free, internalFree);
    EXPECT_EQ(fakeheap::external.free, externalFree);
    EXPECT_TRUE(fakeheap::live.empty());
  }
}

TEST_F(CssArenaBackingTest, LargerSourceAllowanceRequiresPsram) {
  EXPECT_EQ(CssParser::maxSourceBytes(), 512u * 1024u);
  fakeheap::reset(false);
  EXPECT_EQ(CssParser::maxSourceBytes(), 128u * 1024u);
}

TEST_F(CssArenaBackingTest, LargeStreamPreservesHiddenRulesThroughCache) {
  // Publisher comments can make a stylesheet exceed the old 128 KiB limit
  // without needing a large rule map. The parser must reach the hidden rule.
  const std::string text = "/*" + std::string(191327, ' ') + "*/\n.modal { display: none; }";
  ASSERT_LT(text.size(), CssParser::maxSourceBytes());
  Storage.put("large.css", {text.begin(), text.end()});
  FsFile file;
  ASSERT_TRUE(Storage.openFileForRead("test", "large.css", file));
  CssParser css("book");
  ASSERT_EQ(css.loadFromStream(file), CssParser::ParseResult::Complete);
  file.close();
  EXPECT_EQ(css.resolveStyle("div", "modal").display, CssDisplay::None);
  ASSERT_TRUE(css.saveToCache(true));
  css.clear();
  ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
  EXPECT_EQ(css.resolveStyle("div", "modal").display, CssDisplay::None);
}

TEST_F(CssArenaBackingTest, PreviousCacheVersionIsInvalidated) {
  CssParser css("book");
  ASSERT_TRUE(css.saveToCache(true));
  FsFile file;
  // Obtain a valid empty cache, then mark it as the prior cache revision.
  ASSERT_TRUE(Storage.openFileForRead("test", "book/css_rules.cache", file));
  std::vector<uint8_t> bytes(file.size());
  ASSERT_EQ(file.read(bytes.data(), bytes.size()), static_cast<int>(bytes.size()));
  file.close();
  ASSERT_FALSE(bytes.empty());
  bytes[0] = 16;
  Storage.put("book/css_rules.cache", bytes);
  EXPECT_EQ(css.inspectCache(), CssParser::CacheStatus::Invalid);
}

TEST_F(CssArenaBackingTest, FlatParsingMergesSelectorsAndReleasesAllPools) {
  const std::string text = ".modal { display: none; } .MODAL { font-weight: bold; }";
  Storage.put("input.css", {text.begin(), text.end()});
  FsFile file;
  ASSERT_TRUE(Storage.openFileForRead("test", "input.css", file));
  CssParser css("book");
  ASSERT_EQ(css.loadFromStream(file), CssParser::ParseResult::Complete);
  file.close();
  EXPECT_EQ(css.ruleCount(), 1u);
  EXPECT_EQ(css.resolveStyle("div", "modal").display, CssDisplay::None);
  EXPECT_EQ(css.resolveStyle("div", "modal").fontWeight, CssFontWeight::Bold);
  EXPECT_EQ(liveArrayCount, 3u);
  EXPECT_GT(liveArrayBytes, 0u);
  EXPECT_TRUE(fakeheap::live.empty());
  css.clear();
  EXPECT_TRUE(css.empty());
  EXPECT_EQ(css.ruleCount(), 0u);
  EXPECT_EQ(liveArrayCount, 0u);
  EXPECT_EQ(liveArrayBytes, 0u);
  EXPECT_TRUE(fakeheap::live.empty());
}

TEST_F(CssArenaBackingTest, PsramParsingAllocationFailureStopsSafely) {
  const std::string text = ".modal { display: none; }";
  Storage.put("input.css", {text.begin(), text.end()});
  FsFile file;
  ASSERT_TRUE(Storage.openFileForRead("test", "input.css", file));
  CssParser css("book");
  fakeheap::external.fail = 1;
  failCssArrays = true;
  const auto result = css.loadFromStream(file);
  failCssArrays = false;
  EXPECT_NE(result, CssParser::ParseResult::Complete);
  file.close();
  EXPECT_TRUE(css.empty());
  EXPECT_TRUE(fakeheap::live.empty());
  EXPECT_EQ(fakeheap::internal.attempts, 0u);
}

TEST_F(CssArenaBackingTest, EveryHydrationArrayFailureIsLowMemoryAndRetryable) {
  createCache();
  CssParser css("book");
  arrayAttempts = 0;
  ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
  const auto hydrationAttempts = arrayAttempts;
  ASSERT_GE(hydrationAttempts, 4u);  // index, read scratch, selector pool, style pool
  css.clear();
  for (size_t failure = 1; failure <= hydrationAttempts; ++failure) {
    SCOPED_TRACE(failure);
    arrayAttempts = 0;
    failArrayAttempt = failure;
    EXPECT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::LowMemory);
    EXPECT_TRUE(css.empty());
    EXPECT_EQ(css.ruleCount(), 0u);
    EXPECT_EQ(liveArrayCount, 0u);
    EXPECT_EQ(liveArrayBytes, 0u);
    EXPECT_EQ(css.inspectCache(), CssParser::CacheStatus::Complete);
    failArrayAttempt = 0;
    ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
    checkStyle(css);
    css.clear();
    EXPECT_EQ(liveArrayCount, 0u);
  }
}

TEST_F(CssArenaBackingTest, EveryStreamGrowthFailureRetainsOnlyValidPrefixAndReleasesPools) {
  std::string text = ".sentinel { display: none; }\n";
  for (int i = 0; i < 300; ++i)
    text += ".rule" + std::to_string(i) + " { margin-top: " + std::to_string(i % 100) + "px; }\n";
  Storage.put("growth.css", {text.begin(), text.end()});
  CssParser css("book");
  FsFile file;
  ASSERT_TRUE(Storage.openFileForRead("test", "growth.css", file));
  arrayAttempts = 0;
  ASSERT_EQ(css.loadFromStream(file), CssParser::ParseResult::Complete);
  const auto growthAttempts = arrayAttempts;
  ASSERT_GT(growthAttempts, 6u);
  ASSERT_EQ(css.ruleCount(), 301u);
  css.clear();
  for (size_t failure = 1; failure <= growthAttempts; ++failure) {
    SCOPED_TRACE(failure);
    ASSERT_TRUE(Storage.openFileForRead("test", "growth.css", file));
    arrayAttempts = 0;
    failArrayAttempt = failure;
    EXPECT_NE(css.loadFromStream(file), CssParser::ParseResult::Complete);
    EXPECT_LT(css.ruleCount(), 301u);
    if (!css.empty()) EXPECT_EQ(css.resolveStyle("div", "sentinel").display, CssDisplay::None);
    for (size_t i = 0; i + 1 < css.ruleCount(); ++i) {
      const auto style = css.resolveStyle("div", "rule" + std::to_string(i));
      EXPECT_TRUE(style.hasMarginTop());
      EXPECT_EQ(style.marginTop.value, i % 100);
    }
    css.clear();
    EXPECT_EQ(liveArrayCount, 0u);
    EXPECT_EQ(liveArrayBytes, 0u);
    failArrayAttempt = 0;
  }
}

TEST_F(CssArenaBackingTest, FlatIndexAndStyleCapsPublishPartialWithoutLosingPriorStyles) {
  for (bool distinctStyles : {false, true}) {
    SCOPED_TRACE(distinctStyles);
    std::string text;
    const size_t limit = distinctStyles ? 256u : 1500u;
    for (size_t i = 0; i <= limit; ++i)
      text += ".bound" + std::to_string(i) + " { margin-top: " + std::to_string(distinctStyles ? i : 7u) + "px; }\n";
    Storage.put("bounds.css", {text.begin(), text.end()});
    FsFile file;
    ASSERT_TRUE(Storage.openFileForRead("test", "bounds.css", file));
    CssParser css("book");
    EXPECT_EQ(css.loadFromStream(file), CssParser::ParseResult::Partial);
    ASSERT_EQ(css.ruleCount(), limit);
    for (size_t i = 0; i < limit; ++i) {
      const auto style = css.resolveStyle("p", "bound" + std::to_string(i));
      EXPECT_TRUE(style.hasMarginTop());
      EXPECT_EQ(style.marginTop.value, distinctStyles ? i : 7u);
    }
    EXPECT_FALSE(css.resolveStyle("p", "bound" + std::to_string(limit)).hasMarginTop());
    ASSERT_TRUE(css.saveToCache(false));
    css.clear();
    EXPECT_EQ(liveArrayCount, 0u);
    EXPECT_EQ(css.inspectCache(), CssParser::CacheStatus::Partial);
    ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
    EXPECT_EQ(css.ruleCount(), limit);
    EXPECT_EQ(css.resolveStyle("p", "bound0").marginTop.value, distinctStyles ? 0u : 7u);
  }
}

TEST_F(CssArenaBackingTest, InvalidCacheCountsAndSelectorBoundsReleaseAllPools) {
  createCache();
  FsFile file;
  ASSERT_TRUE(Storage.openFileForRead("test", "book/css_rules.cache", file));
  std::vector<uint8_t> valid(file.size());
  ASSERT_EQ(file.read(valid.data(), valid.size()), static_cast<int>(valid.size()));
  ASSERT_GT(valid.size(), 6u);  // version, flags, rule count, first selector length
  for (size_t offset : {2u, 4u}) {
    SCOPED_TRACE(offset);
    auto corrupt = valid;
    corrupt[offset] = corrupt[offset + 1] = 0xFF;
    Storage.put("book/css_rules.cache", corrupt);
    CssParser css("book");
    EXPECT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Invalid);
    EXPECT_TRUE(css.empty());
    EXPECT_EQ(liveArrayCount, 0u);
    EXPECT_EQ(liveArrayBytes, 0u);
  }
}

TEST_F(CssArenaBackingTest, ManyRulesSurviveArenaGrowthAndCacheRoundTrip) {
  std::string text;
  for (int i = 0; i < 1303; ++i) text += ".rule" + std::to_string(i) + " { display: none; }\n";
  Storage.put("input.css", {text.begin(), text.end()});
  FsFile file;
  ASSERT_TRUE(Storage.openFileForRead("test", "input.css", file));
  CssParser css("book");
  ASSERT_EQ(css.loadFromStream(file), CssParser::ParseResult::Complete);
  file.close();
  ASSERT_EQ(css.ruleCount(), 1303u);
  for (int i = 0; i < 1303; ++i)
    EXPECT_EQ(css.resolveStyle("div", "rule" + std::to_string(i)).display, CssDisplay::None);
  ASSERT_TRUE(css.saveToCache(true));
  css.clear();
  ASSERT_EQ(css.loadFromCache(), CssParser::CacheLoadResult::Complete);
  ASSERT_EQ(css.ruleCount(), 1303u);
  for (int i = 0; i < 1303; ++i)
    EXPECT_EQ(css.resolveStyle("div", "rule" + std::to_string(i)).display, CssDisplay::None);
}
