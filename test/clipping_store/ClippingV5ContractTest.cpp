#include <ClippingStore.h>
#include <HalStorage.h>
#include <Serialization.h>
#include <gtest/gtest.h>

#include <cstring>

namespace {
using Result = ClippingStore::AddResult;
std::string snapshotPath() {
  for (const auto& [path, node] : fake::files)
    if (path.ends_with(".bin")) return path;
  return "";
}
Clipping row(unsigned i) {
  Clipping c;
  snprintf(c.id, sizeof(c.id), "row-%u", i);
  c.spineIndex = i;
  c.startPage = i + 1;
  c.endPage = i + 2;
  c.pageCount = 100;
  c.startWordIndex = i + 3;
  c.endWordIndex = i + 4;
  c.wordCount = 2;
  c.paragraphIndex = i + 5;
  c.startOffset = i * 100;
  c.endOffset = c.startOffset + 20;
  c.tableSelection = i + 6;
  c.layoutSignature = 123;
  c.syncRevision = 10;
  return c;
}
void expectRow(ClippingStore& store, size_t at, unsigned i) {
  const auto expected = row(i);
  const auto* actual = store.clippingAt(at);
  ASSERT_NE(actual, nullptr);
  EXPECT_STREQ(actual->id, expected.id);
  EXPECT_EQ(actual->spineIndex, expected.spineIndex);
  EXPECT_EQ(actual->startPage, expected.startPage);
  EXPECT_EQ(actual->endPage, expected.endPage);
  EXPECT_EQ(actual->pageCount, expected.pageCount);
  EXPECT_EQ(actual->startWordIndex, expected.startWordIndex);
  EXPECT_EQ(actual->endWordIndex, expected.endWordIndex);
  EXPECT_EQ(actual->paragraphIndex, expected.paragraphIndex);
  EXPECT_EQ(actual->tableSelection, expected.tableSelection);
  EXPECT_EQ(actual->startOffset, expected.startOffset);
  EXPECT_EQ(actual->endOffset, expected.endOffset);
  EXPECT_EQ(actual->layoutSignature, expected.layoutSignature);
  EXPECT_EQ(actual->syncRevision, expected.syncRevision);
  EXPECT_FALSE(actual->pendingUpload);
  EXPECT_TRUE(clippingContainsWord(*actual, i, 0, 0, 0, 0, i * 100, i * 100 + 1));
  EXPECT_FALSE(clippingContainsWord(*actual, i, 0, 0, 0, 0, i * 100 + 20, i * 100 + 21));
  std::string text;
  ASSERT_TRUE(store.readClippingText(at, text));
  EXPECT_EQ(text, std::to_string(i));
}
void legacySnapshot(const std::string& path) {
  HalFile f = Storage.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  ASSERT_TRUE(f);
  ASSERT_TRUE(serialization::tryWritePod(f, uint8_t{2}));
  ASSERT_TRUE(serialization::tryWritePod(f, uint16_t{0}));
  ASSERT_TRUE(serialization::tryWriteString(f, "Book"));
  ASSERT_TRUE(serialization::tryWriteString(f, "Author"));
  ASSERT_TRUE(serialization::tryWriteString(f, "/book.epub"));
  ASSERT_TRUE(f.close());
}
}  // namespace

TEST(ClippingV5Contract, GrowthAndFailedDeletionPreserveAllLogicalBlockLocations) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  for (unsigned i = 0; i < 33; ++i) {
    ASSERT_TRUE(store.applyRemote(row(i), std::to_string(i), false));
    // Reacquire by index, as reader callers do; never dereference an invalidated vector pointer.
    for (unsigned j = 0; j <= i; ++j) expectRow(store, j, j);
  }
  const auto path = snapshotPath();
  const auto original = fake::files.at(path)->bytes;
  fake::failWrite = 1;
  ASSERT_FALSE(store.removeClippingAt(15));
  EXPECT_TRUE(fake::failureTriggered);
  ASSERT_EQ(store.clippingCount(), 33u);
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  for (unsigned i = 0; i < 33; ++i) expectRow(store, i, i);
  ASSERT_TRUE(store.removeClippingAt(15));
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_EQ(store.clippingCount(), 32u);
  for (unsigned i = 0; i < 32; ++i) expectRow(store, i, i < 15 ? i : i + 1);
  uint32_t offset = 0;
  char id[65];
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_STREQ(id, "row-15");
}

TEST(ClippingV5Contract, ShortSnapshotWriteRollsBackRecordAndDurableTombstone) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_TRUE(store.applyRemote(row(0), "0", false));
  ASSERT_TRUE(store.applyRemote(row(1), "1", false));
  ASSERT_TRUE(store.removeClippingAt(0));
  const auto path = snapshotPath();
  const auto before = fake::files.at(path)->bytes;
  fake::shortWrite = 0;
  ASSERT_FALSE(store.removeClippingAt(0));
  ASSERT_EQ(store.clippingCount(), 1u);
  expectRow(store, 0, 1);
  EXPECT_EQ(fake::files.at(path)->bytes, before);
  EXPECT_FALSE(Storage.exists((path + ".tmp").c_str()));
  uint32_t offset = 0;
  char id[65];
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_STREQ(id, "row-0");
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_EQ(id[0], 0);
  ASSERT_TRUE(store.removeClippingAt(0));
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(store.clippingCount(), 0u);
  offset = 0;
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_STREQ(id, "row-0");
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_STREQ(id, "row-1");
  ASSERT_TRUE(store.applyRemote(row(1), "stale replay", false));
  EXPECT_EQ(store.clippingCount(), 0u);
  fake::failWrite = 0;
  EXPECT_FALSE(store.finishDeletions());
  offset = 0;
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_STREQ(id, "row-0");
  ASSERT_TRUE(store.finishDeletions());
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  offset = 0;
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_EQ(id[0], 0);
  offset = 1;
  EXPECT_FALSE(store.nextDeletion(offset, id));
  EXPECT_EQ(offset, 1u);
}

TEST(ClippingV5Contract, LegacyJournalImportIsReadOnlyAndPreservesInterruptedTail) {
  for (const bool interrupted : {false, true}) {
    fake::reset();
    ClippingStore store;
    ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    ASSERT_TRUE(store.applyRemote(row(0), "0", false));
    const auto path = snapshotPath();
    store.unload();
    legacySnapshot(path);
    const auto before = fake::files.at(path)->bytes;
    std::string journal(65, '\0');
    journal.replace(0, 7, "deleted");
    if (interrupted) journal += 'x';
    fake::add(path + ".deleted", journal);
    EXPECT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, std::vector<uint8_t>(journal.begin(), journal.end()));
    EXPECT_EQ(fake::files.at(path)->bytes, before);
    uint32_t offset = 0;
    char id[65];
    ASSERT_TRUE(store.prepareSync());
    EXPECT_EQ(fake::files.at(path)->bytes[0], 5);
    EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, std::vector<uint8_t>(journal.begin(), journal.end()));
    store.unload();
    ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    ASSERT_TRUE(store.nextDeletion(offset, id));
    EXPECT_STREQ(id, "deleted");
    ASSERT_TRUE(store.finishDeletions());
    EXPECT_FALSE(Storage.exists((path + ".deleted").c_str()));
  }
}

TEST(ClippingV5Contract, BoundsAndSyncStateSurviveRollbackAndReload) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  const std::string text(CLIPPING_TEXT_MAX, 'x');
  ASSERT_EQ(store.addClipping(1, 2, 3, 10, 4, 5, 6, "Chapter", 7, text, uint32_t{8}, 100, 200), Result::Added);
  const auto path = snapshotPath();
  const auto before = fake::files.at(path)->bytes;
  EXPECT_EQ(store.addClipping(1, 2, 3, 10, 4, 5, 6, "Chapter", 7, text + "x", uint32_t{8}, 100, 200),
            Result::SaveFailed);
  EXPECT_EQ(store.addClipping(1, 2, 3, 10, 4, 5, 6, "Chapter", 7, text, uint32_t{8}, 200, 100), Result::SaveFailed);
  EXPECT_EQ(store.clippingCount(), 1u);
  EXPECT_EQ(fake::files.at(path)->bytes, before);
  ASSERT_TRUE(store.prepareSync());
  const std::string id = store.clippingAt(0)->id;
  ASSERT_FALSE(id.empty());
  fake::failWrite = 0;
  EXPECT_FALSE(store.finishUploads());
  EXPECT_TRUE(store.clippingAt(0)->pendingUpload);
  ASSERT_TRUE(store.finishUploads());
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  const Clipping saved = *store.clippingAt(0);
  EXPECT_STREQ(saved.id, id.c_str());
  EXPECT_FALSE(saved.pendingUpload);
  EXPECT_EQ(saved.startOffset, 100u);
  EXPECT_EQ(saved.endOffset, 200u);
  EXPECT_EQ(saved.startPage, 2);
  EXPECT_EQ(saved.endPage, 3);
  EXPECT_EQ(saved.paragraphIndex, 7);
  std::string actual;
  ASSERT_TRUE(store.readClippingText(0, actual));
  EXPECT_EQ(actual, text);
  Clipping remote = saved;
  remote.syncRevision = 20;
  ASSERT_TRUE(store.applyRemote(remote, "new revision", false));
  remote.syncRevision = 19;
  ASSERT_TRUE(store.applyRemote(remote, "stale revision", false));
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(store.clippingAt(0)->syncRevision, 20u);
  ASSERT_TRUE(store.readClippingText(0, actual));
  EXPECT_EQ(actual, "new revision");
}

TEST(ClippingV5Contract, RetainedPublicationRootsCannotBeDeletedOrLoaded) {
  for (const char* suffix : {".tmp", ".bak", ".rename.bak", ".migrate.bak"}) {
    fake::reset();
    ClippingStore store;
    ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    ASSERT_TRUE(store.applyRemote(row(0), "0", false));
    store.unload();
    const auto path = snapshotPath();
    const auto before = fake::files.at(path)->bytes;
    fake::add(path + suffix, "retained");
    EXPECT_FALSE(ClippingStore::deleteForFilePath("/book.epub", "epub"));
    EXPECT_FALSE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    EXPECT_EQ(store.clippingCount(), 0u);
    EXPECT_FALSE(store.prepareSync());
    EXPECT_EQ(fake::files.at(path)->bytes, before);
    EXPECT_EQ(fake::files.at(path + suffix)->bytes, std::vector<uint8_t>({'r', 'e', 't', 'a', 'i', 'n', 'e', 'd'}));
  }
}
