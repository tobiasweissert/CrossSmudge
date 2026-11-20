#include <ClippingStore.h>
#include <HalStorage.h>
#include <Serialization.h>
#include <gtest/gtest.h>

namespace {
using Result = ClippingStore::AddResult;
Result add(ClippingStore& store, const std::string& text = "one", const char* title = "Chapter") {
  return store.addClipping(0, 0, 0, 1, 0, 0, 1, title, 0, text, 1, 0, 3);
}
std::string storePath() {
  for (const auto& [path, node] : fake::files)
    if (path.ends_with(".bin")) return path;
  return "";
}
}  // namespace

TEST(ClippingStore, FailedLoadCannotPublishOrOverwriteValidPrefix) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_EQ(add(store), Result::Added);
  ASSERT_EQ(add(store, "two"), Result::Added);
  store.unload();
  auto& bytes = fake::files.at(storePath())->bytes;
  bytes.pop_back();
  const auto damaged = bytes;
  EXPECT_FALSE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(store.clippingCount(), 0u);
  EXPECT_EQ(add(store), Result::SaveFailed);
  EXPECT_FALSE(store.saveToFile());
  EXPECT_FALSE(store.prepareSync());
  store.unload();
  EXPECT_EQ(bytes, damaged);
  ASSERT_TRUE(store.loadForBook("/other.epub", "Other", "", "epub"));
  ASSERT_EQ(add(store), Result::Added);
  EXPECT_FALSE(store.loadForBook("/bad.txt", "", "", "unsupported"));
  EXPECT_EQ(store.clippingCount(), 0u);
  EXPECT_EQ(add(store), Result::SaveFailed);
}

TEST(ClippingStore, AllocationFailureKeepsOldIndexAndUnloadReleasesIt) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  for (int i = 0; i < 16; ++i) ASSERT_EQ(add(store), Result::Added);
  fake::failAlloc = 0;
  EXPECT_EQ(add(store), Result::SaveFailed);
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_EQ(store.clippingCount(), 16u);
  std::string text;
  EXPECT_TRUE(store.readClippingText(3, text));
  EXPECT_EQ(text, "one");
  store.unload();
  EXPECT_EQ(store.clippingAt(0), nullptr);
  EXPECT_EQ(fake::liveCheckedAllocations, 0u);
  EXPECT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(store.clippingCount(), 16u);
}

TEST(ClippingStore, CountLimitAndHeaderLimitsRoundTrip) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  for (unsigned i = 0; i < CLIPPING_MAX_PER_BOOK - 1; ++i) ASSERT_EQ(add(store), Result::Added);
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(add(store), Result::Added);
  EXPECT_EQ(add(store), Result::LimitReached);
  store.unload();
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(store.clippingCount(), CLIPPING_MAX_PER_BOOK);
  const auto original = fake::files.at(storePath())->bytes;
  store.unload();
  ASSERT_FALSE(store.loadForBook("/book.epub", std::string(CLIPPING_TEXT_MAX + 1, 'x'), "Author", "epub"));
  EXPECT_FALSE(store.removeClippingAt(0));
  store.unload();
  EXPECT_EQ(fake::files.at(storePath())->bytes, original);
}

TEST(ClippingStore, GrowthKeepsRecordsInPlaceAndDeletionRollsBackAcrossBlocks) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));

  for (unsigned i = 0; i < 33; ++i) {
    Clipping clipping;
    snprintf(clipping.id, sizeof(clipping.id), "clip-%u", i);
    const std::string text = std::to_string(i);
    clipping.textLength = text.size();
    ASSERT_TRUE(store.applyRemote(clipping, text, false));
    // Callers reacquire indices after mutation; physical vector addresses are
    // not part of their contract. Preserve identity and text instead.
    ASSERT_NE(store.clippingAt(0), nullptr);
    EXPECT_STREQ(store.clippingAt(0)->id, "clip-0");
    std::string firstText;
    ASSERT_TRUE(store.readClippingText(0, firstText));
    EXPECT_EQ(firstText, "0");
  }
  const auto path = storePath();
  const auto original = fake::files.at(path)->bytes;
  // The journal succeeds; the store rewrite fails after records have shifted.
  fake::failWrite = 1;
  EXPECT_FALSE(store.removeClippingAt(15));
  ASSERT_EQ(store.clippingCount(), 33u);
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  for (unsigned i = 0; i < 33; ++i) {
    std::string text;
    ASSERT_TRUE(store.readClippingText(i, text));
    EXPECT_EQ(text, std::to_string(i));
  }
  const Clipping removed = *store.clippingAt(15);
  fake::failWrite = 0;
  EXPECT_FALSE(store.applyRemote(removed, "", true));
  EXPECT_EQ(store.clippingCount(), 33u);
  ASSERT_TRUE(store.applyRemote(removed, "", true));
  ASSERT_TRUE(store.removeClippingAt(15));
  store.unload();
  // Fail partway through allocating blocks while loading, then retry.
  fake::failAlloc = 1;
  EXPECT_FALSE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_EQ(store.clippingCount(), 0u);
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_EQ(store.clippingCount(), 31u);
  for (unsigned i = 0; i < 31; ++i) {
    std::string text;
    ASSERT_TRUE(store.readClippingText(i, text));
    EXPECT_EQ(text, std::to_string(i < 15 ? i : i + 2));
  }
}

TEST(ClippingStore, OffsetAllocationFailureDoesNotCreateTempFile) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_EQ(add(store), Result::Added);
  const auto path = storePath();
  const auto original = fake::files.at(path)->bytes;
  fake::failAlloc = 0;
  EXPECT_EQ(add(store, "two"), Result::SaveFailed);
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_EQ(store.clippingCount(), 1u);
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  EXPECT_FALSE(Storage.exists((path + ".tmp").c_str()));
  EXPECT_EQ(add(store, "two"), Result::Added);
}

TEST(ClippingStore, ShortDeletionAppendRollsBackAndRetryCanSync) {
  // v5 commits queue and rows together, not via generated .deleted appends.
  for (const int boundary : {8, 9}) {
    fake::reset();
    ClippingStore store;
    ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    ASSERT_EQ(add(store), Result::Added);
    ASSERT_EQ(add(store, "two"), Result::Added);
    ASSERT_TRUE(store.prepareSync());
    const std::string firstId = store.clippingAt(0)->id;
    const std::string secondId = store.clippingAt(1)->id;
    ASSERT_TRUE(store.removeClippingAt(0));
    const auto path = storePath();
    const auto original = fake::files.at(path)->bytes;
    fake::failWrite = boundary;
    EXPECT_FALSE(store.removeClippingAt(0));
    EXPECT_TRUE(fake::failureTriggered);
    ASSERT_EQ(store.clippingCount(), 1u);
    EXPECT_EQ(fake::files.at(path)->bytes, original);
    EXPECT_FALSE(Storage.exists((path + ".tmp").c_str()));
    ASSERT_TRUE(store.removeClippingAt(0));
    store.unload();
    ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    uint32_t offset = 0;
    char id[65];
    ASSERT_TRUE(store.nextDeletion(offset, id));
    EXPECT_EQ(id, firstId);
    ASSERT_TRUE(store.nextDeletion(offset, id));
    EXPECT_EQ(id, secondId);
    ASSERT_TRUE(store.nextDeletion(offset, id));
    EXPECT_EQ(id[0], '\0');
    EXPECT_FALSE(Storage.exists((path + ".deleted").c_str()));
    EXPECT_TRUE(store.finishDeletions());
  }
}

TEST(ClippingStore, InterruptedDeletionTailRecoversBeforeReading) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_EQ(add(store), Result::Added);
  const auto path = storePath();
  store.unload();
  // Genuine empty modern/owner v4 header, not a v5-generated journal.
  HalFile f = Storage.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  ASSERT_TRUE(serialization::tryWritePod(f, uint8_t{4}));
  ASSERT_TRUE(serialization::tryWritePod(f, uint16_t{0}));
  ASSERT_TRUE(serialization::tryWriteString(f, "Book"));
  ASSERT_TRUE(serialization::tryWriteString(f, "Author"));
  ASSERT_TRUE(serialization::tryWriteString(f, "/book.epub"));
  ASSERT_TRUE(f.close());
  std::string journal(65, '\0');
  journal.replace(0, 7, "deleted");
  journal += 'x';
  fake::add(path + ".deleted", journal);
  const auto original = fake::files.at(path)->bytes;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  ASSERT_TRUE(store.prepareSync());
  uint32_t offset = 0;
  char id[65];
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_STREQ(id, "deleted");
  EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, std::vector<uint8_t>(journal.begin(), journal.end()));
  ASSERT_TRUE(store.nextDeletion(offset, id));
  EXPECT_EQ(id[0], '\0');
  ASSERT_TRUE(store.finishDeletions());
  EXPECT_FALSE(Storage.exists((path + ".deleted").c_str()));
}

TEST(ClippingStore, Utf8TitleAndFailedDeletionPreserveRecord) {
  fake::reset();
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
  ASSERT_EQ(add(store, "one", (std::string(46, 'x') + "가").c_str()), Result::Added);
  EXPECT_EQ(std::string(store.clippingAt(0)->chapterTitle), std::string(46, 'x'));
  fake::failWrite = 0;
  EXPECT_FALSE(store.removeClippingAt(0));
  ASSERT_EQ(store.clippingCount(), 1u);
  std::string text;
  EXPECT_TRUE(store.readClippingText(0, text));
  EXPECT_EQ(text, "one");
}

TEST(ClippingStore, MovesFolderWithClipsAndDeletionJournalAndRollsBackFailure) {
  for (const int failure : {-1, 0, 1, 2, 3}) {
    fake::reset();
    fake::add("/books/sub/book.epub");
    ClippingStore store;
    ASSERT_TRUE(store.loadForBook("/books/sub/book.epub", "Book", "Author", "epub"));
    ASSERT_EQ(add(store), Result::Added);
    ASSERT_EQ(add(store, "two"), Result::Added);
    ASSERT_TRUE(store.prepareSync());
    ASSERT_TRUE(store.removeClippingAt(0));
    store.unload();
    const auto oldStore = storePath();
    const auto original = fake::files.at(oldStore)->bytes;
    EXPECT_FALSE(Storage.exists((oldStore + ".deleted").c_str()));
    fake::failRename = failure;
    const bool moved = ClippingStore::moveBook("/books", "/renamed");
    if (moved) {
      EXPECT_FALSE(Storage.exists("/books/sub/book.epub"));
      EXPECT_TRUE(Storage.exists("/renamed/sub/book.epub"));
      EXPECT_FALSE(Storage.exists(oldStore.c_str()));
      ASSERT_TRUE(store.loadForBook("/renamed/sub/book.epub", "", "", "epub"));
      EXPECT_EQ(store.clippingCount(), 1u);
      std::string text;
      EXPECT_TRUE(store.readClippingText(0, text));
      EXPECT_EQ(text, "two");
      uint32_t offset = 0;
      char id[65];
      EXPECT_TRUE(store.nextDeletion(offset, id));
      EXPECT_NE(id[0], 0);
    } else {
      EXPECT_TRUE(Storage.exists("/books/sub/book.epub"));
      EXPECT_FALSE(Storage.exists("/renamed/sub/book.epub"));
      EXPECT_EQ(fake::files.at(oldStore)->bytes, original);
      EXPECT_TRUE(fake::failureTriggered);
      EXPECT_TRUE(Storage.exists("/.crosspoint/clippings/move.pending"));
      EXPECT_FALSE(ClippingStore::moveBook("/books", "/renamed"));
      EXPECT_FALSE(ClippingStore::deleteForFilePath("/books/sub/book.epub", "epub"));
    }
  }
}

TEST(ClippingStore, MoveRefusesExistingDestinationHistoryAndDeleteCleansSidecars) {
  fake::reset();
  fake::add("/one.epub");
  ClippingStore store;
  ASSERT_TRUE(store.loadForBook("/one.epub", "One", "", "epub"));
  ASSERT_EQ(add(store), Result::Added);
  store.unload();
  ASSERT_TRUE(store.loadForBook("/two.epub", "Two", "", "epub"));
  ASSERT_EQ(add(store, "two"), Result::Added);
  store.unload();
  EXPECT_FALSE(ClippingStore::moveBook("/one.epub", "/two.epub"));
  EXPECT_TRUE(Storage.exists("/one.epub"));
  EXPECT_TRUE(ClippingStore::deleteForFilePath("/two.epub", "epub"));
  EXPECT_TRUE(ClippingStore::moveBook("/one.epub", "/two.epub"));
  const auto path = storePath();
  fake::add(path + ".deleted", std::string(65, 'x'));
  fake::add(path + ".bak");
  fake::add(path + ".tmp");
  const auto original = fake::files.at(path)->bytes;
  EXPECT_FALSE(ClippingStore::deleteForFilePath("/two.epub", "epub"));
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  EXPECT_TRUE(Storage.exists((path + ".deleted").c_str()));
  EXPECT_TRUE(Storage.exists((path + ".bak").c_str()));
  EXPECT_TRUE(Storage.exists((path + ".tmp").c_str()));
}
