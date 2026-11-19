#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <iterator>

#include "Epub.h"
#include "HalStorage.h"
#include "LibraryBuilder.h"
#include "LibraryIndexFile.h"
#include "ZipFile.h"

static void installUpstreamFixture() {
  std::ifstream file(LIBRARY_FIXTURE_PATH, std::ios::binary);
  std::string bytes((std::istreambuf_iterator<char>(file)), {});
  ASSERT_GT(bytes.size(), 64u);
  fake::add("/.crosspoint/library.idx", bytes);
  fake::add("/b.txt", "b", 2);
  fake::add("/a.txt", "a", 3);
}
using namespace library;
TEST(CrosscubMigration, UsesDistinctDerivedNamespace) {
  EXPECT_STREQ(libraryIndexPath(), "/.crosspoint/crosscub-library-v7.idx");
  EXPECT_EQ(CLIX_FORMAT_VERSION, 7);
  EXPECT_EQ(CLIX_FOLD_VERSION, 6);
}
TEST(CrosscubMigration, ExcludesPrivateAppFontSleepAndRecoveryPaths) {
  fake::reset();
  bookMetadata.clear();
  for (auto p : {"/books/book.txt", "/apps/game/save.txt", "/fonts/readme.md", "/sleep/private.txt",
                 "/.airdrop-receive/test.epub", "/.crosspoint/recovery/book.epub", "/apps/browser/cache/secret.txt",
                 "/.hidden/test.txt", "/Books/.receive.txt"})
    fake::add(p);
  BuildStats stats;
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(stats.books, 1);
  LibraryIndexFile idx;
  ASSERT_TRUE(idx.open(libraryIndexPath()));
  ClixRecord rec{};
  std::string path;
  ASSERT_TRUE(idx.readRecord(0, rec));
  ASSERT_TRUE(idx.readPath(rec, path));
  EXPECT_EQ(path, "/books/book.txt");
}
TEST(CrosscubMigration, DerivedScanDoesNotTouchDurableBookOrAppData) {
  fake::reset();
  bookMetadata.clear();
  for (auto p : {"/books/book.epub", "/.crosspoint/recent.json", "/.crosspoint/cache_1/progress",
                 "/.crosspoint/cache_1/bookmarks", "/.crosspoint/cache_1/clippings",
                 "/.crosspoint/cache_1/reading_stats", "/.crosspoint/cache_1/reading_stats_off", "/apps/game/save.txt"})
    fake::add(p, "durable-canary");
  std::map<std::string, std::vector<uint8_t>> before;
  for (const auto& [p, n] : fake::files)
    if (!n->directory) before[p] = n->bytes;
  BuildStats stats;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  for (const auto& [p, b] : before) {
    ASSERT_TRUE(fake::files.count(p));
    EXPECT_EQ(fake::files[p]->bytes, b) << p;
  }
}
TEST(CrosscubMigration, GenuineUpstreamTwoMigratesWithoutChangingLegacyOrArrivalHistory) {
  fake::reset();
  installUpstreamFixture();
  const auto before = fake::files["/.crosspoint/library.idx"]->bytes;
  LibraryIndexFile old;
  ASSERT_TRUE(old.openForReconciliation("/.crosspoint/library.idx"));
  EXPECT_EQ(old.header().formatVersion, 2);
  EXPECT_EQ(old.header().foldVersion, 5);
  std::map<std::string, uint16_t> seen;
  for (uint16_t i = 0; i < old.bookCount(); ++i) {
    ClixRecord rec{};
    std::string p;
    ASSERT_TRUE(old.readRecord(i, rec));
    ASSERT_TRUE(old.readPath(rec, p));
    seen[p] = rec.firstSeen;
  }
  old.close();
  BuildStats stats;
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::files["/.crosspoint/library.idx"]->bytes, before);
  LibraryIndexFile now;
  ASSERT_TRUE(now.open(libraryIndexPath()));
  EXPECT_EQ(now.header().formatVersion, 7);
  EXPECT_EQ(now.bookCount(), 2);
  for (uint16_t i = 0; i < now.bookCount(); ++i) {
    ClixRecord rec{};
    std::string p;
    ASSERT_TRUE(now.readRecord(i, rec));
    ASSERT_TRUE(now.readPath(rec, p));
    EXPECT_EQ(rec.firstSeen, seen[p]) << p;
  }
  EXPECT_GT(seen["/a.txt"], seen["/b.txt"]);
}
TEST(CrosscubMigration, CancelledOrFailedFirstMigrationLeavesGenuineUpstreamReadable) {
  for (bool cancel : {false, true}) {
    fake::reset();
    installUpstreamFixture();
    const auto before = fake::files["/.crosspoint/library.idx"]->bytes;
    BuildCallbacks cb;
    cb.cancelRequested = [](void*) { return true; };
    if (!cancel) fake::failWrite = 0;
    BuildStats stats;
    EXPECT_FALSE(buildLibraryIndex("/", stats, false, cancel ? &cb : nullptr));
    EXPECT_EQ(fake::files["/.crosspoint/library.idx"]->bytes, before);
    EXPECT_FALSE(Storage.exists(libraryIndexPath()));
    LibraryIndexFile old;
    ASSERT_TRUE(old.openForReconciliation(libraryRetainedIndexPath()));
    EXPECT_EQ(old.bookCount(), 2);
  }
}
TEST(CrosscubMigration, UpstreamMutationApiInvalidatesTheSelectedIndex) {
  restoreLibraryIndexAfterSleep();
  EXPECT_FALSE(isLibraryIndexDirty());
  EXPECT_TRUE(markLibraryIndexDirty());
  EXPECT_TRUE(libraryIndexNeedsRefresh());
  EXPECT_TRUE(isLibraryBookFile("/Books/Test.EPUB"));
  EXPECT_FALSE(isLibraryBookFile("/APPS/game/save.TXT"));
  EXPECT_FALSE(isLibraryBookFile("/books/.hidden/book.md"));
}
