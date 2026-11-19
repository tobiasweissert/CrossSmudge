#include <FreeInkUI.h>  // Actual pinned SDK declarations, never a fixture copy.
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <vector>
#define private public
#include "activities/reader/DictionaryDefinitionActivity.h"
#undef private
#include "util/DictionaryRegistry.h"
#include "vocabulary/VocabularyStore.h"

namespace {
std::string selected;
bool selectionFile = false;
unsigned selectionWrites = 0;
void install(const std::string& folder) {
  fake::add("/.dictionaries/" + folder + "/data.idx", "index");
  fake::add("/.dictionaries/" + folder + "/data.dict", "definition");
}
std::string contents(const std::string& path) {
  const auto& bytes = fake::files.at(path)->bytes;
  return {bytes.begin(), bytes.end()};
}
class Compatibility : public testing::Test {
  void SetUp() override {
    fake::reset();
    selected.clear();
    selectionFile = false;
    selectionWrites = 0;
  }
};
}  // namespace
std::string Dictionary::readConfiguredDictPath(const char*) { return selected; }
bool Dictionary::hasGlobalDictPathFile() { return selectionFile; }
void Dictionary::saveGlobalDictPath(const char* path) {
  selected = path;
  selectionFile = true;
  ++selectionWrites;
}
DictInfo Dictionary::readInfo(const char*) { return {}; }
DictDefinitionSlice Dictionary::resolveDefinitionSlice(const DictLocation& location, const DictInfo&) {
  return {location.folderPath, location.offset, location.size, 'm', false, location.found};
}
// These are framework lifetime doubles only; source parsing and storage below are real.
DictHtmlRenderer::DictHtmlRenderer() = default;
DictHtmlRenderer::~DictHtmlRenderer() = default;

TEST_F(Compatibility, OutputDiscoveryReturnsFreshSortedEntriesWithoutChangingSelection) {
  install("Zulu");
  install("alpha");
  selected = "/.dictionaries/Zulu/data";
  selectionFile = true;
  const DictionaryRegistry registry;
  std::vector<DictionaryEntry> out{{"stale", "stale", "stale"}};
  EXPECT_TRUE(registry.discover(out));
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].name, "alpha");
  EXPECT_EQ(out[1].basePath, selected);
  EXPECT_EQ(selectionWrites, 0u);
  EXPECT_EQ(registry.count(), 0);
  fake::reset();
  EXPECT_FALSE(DictionaryRegistry::discover(out));
  EXPECT_TRUE(out.empty());
}
TEST_F(Compatibility, CachedDiscoveryKeepsExplicitNoneAndExistingDictionary) {
  install("alpha");
  install("Zulu");
  DictionaryRegistry registry;
  selectionFile = true;
  EXPECT_TRUE(registry.discover());
  EXPECT_TRUE(selected.empty());
  EXPECT_EQ(selectionWrites, 0u);
  selected = "/.dictionaries/Zulu/data";
  EXPECT_TRUE(registry.discover());
  EXPECT_EQ(selected, "/.dictionaries/Zulu/data");
  EXPECT_EQ(selectionWrites, 0u);
  selected = "/missing/data";
  EXPECT_TRUE(registry.discover());
  EXPECT_EQ(selected, "/.dictionaries/alpha/data");
}
TEST_F(Compatibility, LegacyTextConstructorOwnsLiteralTextAndHtmlFlag) {
  GfxRenderer renderer;
  MappedInputManager input;
  DictionaryDefinitionActivity literal(renderer, input, "Title", std::string("<b>literal</b>"));
  literal.prepareDefinitionSource();
  EXPECT_EQ(literal.inlineDefinitionText_, "<b>literal</b>");
  EXPECT_FALSE(literal.definitionReadFailed_);
  EXPECT_FALSE(literal.definitionIsHtml_);
  EXPECT_FALSE(literal.showLookupButton);
  DictionaryDefinitionActivity html(renderer, input, "About", "<h2>Heading</h2><p>Meaning &amp; text</p>", true);
  html.prepareDefinitionSource();
  EXPECT_NE(html.inlineDefinitionText_.find("Heading"), std::string::npos);
  EXPECT_NE(html.inlineDefinitionText_.find("Meaning & text"), std::string::npos);
  EXPECT_EQ(html.inlineDefinitionText_.find('<'), std::string::npos);
  EXPECT_FALSE(html.showLookupButton);
  html.linesPerPage = 2;
  html.collectTargetPage_ = 0;
  html.wrapPlain();
  ASSERT_FALSE(html.layoutLines.empty());
  EXPECT_NE(html.pagePool_.find("Heading"), std::string::npos);
  EXPECT_NE(html.pagePool_.find("Meaning & text"), std::string::npos);
  EXPECT_EQ(fake::reads, 0u);
  // Reflow must not interpret already decoded '<' text as HTML a second time.
  DictionaryDefinitionActivity escaped(renderer, input, "Escaped", "<p>&lt;b&gt;literal&lt;/b&gt;</p>", true);
  escaped.prepareDefinitionSource();
  escaped.prepareDefinitionSource();
  EXPECT_NE(escaped.inlineDefinitionText_.find("<b>literal</b>"), std::string::npos);
}
TEST_F(Compatibility, LocationConstructorPreservesBookContextSelectionAndHeadword) {
  GfxRenderer renderer;
  MappedInputManager input;
  DictLocation location{"/.dictionaries/Zulu/data", "Canonical", 42, 17, true, false};
  DictionaryDefinitionActivity viewer(renderer, input, "Canonical", location, true, "/book/cache");
  viewer.prepareDefinitionSource();
  EXPECT_EQ(viewer.cachePath, "/book/cache");
  EXPECT_TRUE(viewer.showLookupButton);
  EXPECT_EQ(viewer.headword, "Canonical");
  EXPECT_EQ(viewer.foundLocation.headword, "Canonical");
  EXPECT_EQ(viewer.definitionOffset_, 42u);
  EXPECT_EQ(viewer.definitionSize_, 17u);
  EXPECT_FALSE(viewer.inlineDefinition_);
}
TEST_F(Compatibility, VocabularyHtmlExportKeepsHeadwordContextAndSavedRecords) {
  const std::string old = "{\"v\":1,\"word\":\"saved\"}\n";
  fake::add(VocabularyStore::FILE_PATH, old);
  VocabularyContext context;
  context.title = "A book";
  context.chapter = "Chapter";
  EXPECT_TRUE(VocabularyStore::append(context, "running", "Run", "<h2>Run</h2><p>Move &amp; go</p>", true));
  const auto saved = contents(VocabularyStore::FILE_PATH);
  EXPECT_EQ(saved.substr(0, old.size()), old);
  EXPECT_NE(saved.find("\"word\":\"running\""), std::string::npos);
  EXPECT_NE(saved.find("\"headword\":\"Run\""), std::string::npos);
  EXPECT_NE(saved.find("Move & go"), std::string::npos);
  EXPECT_NE(saved.find("\"title\":\"A book\""), std::string::npos);
  EXPECT_EQ(saved.find("<h2>"), std::string::npos);
  EXPECT_EQ(saved.find("\\u0001"), std::string::npos);
}
TEST_F(Compatibility, VocabularyFailedAppendDoesNotOverwriteSavedEntries) {
  const std::string old = "{\"v\":1,\"word\":\"saved\"}\n";
  fake::add(VocabularyStore::FILE_PATH, old);
  fake::shortWrite = 7;
  EXPECT_FALSE(VocabularyStore::append({}, "word", "Word", "definition", false));
  EXPECT_EQ(contents(VocabularyStore::FILE_PATH), old);
}
