#include "ReviewMoveFixtures.h"
TEST(ClippingBookMove, CleanV5TombstonesStillMoveThroughActualCaller) {
  reviewMove::cleanTombstones(
      [](const std::string& from, const std::string& to) {
        return BookMoveUtils::migrateRenamedBookState(from, to, "", "", "", "epub", true) ==
               BookMoveUtils::RenameMigrationResult::Success;
      },
      false);
}
TEST(ClippingBookMove, RawLegacyJournalRefusesActualCallerUntilAcknowledged) {
  reviewMove::legacyMove(
      [](const std::string& from, const std::string& to) {
        return BookMoveUtils::migrateRenamedBookState(from, to, "", "", "", "epub", true) ==
               BookMoveUtils::RenameMigrationResult::Success;
      },
      false);
}
namespace {
void seedBookMove(const std::string& path) {
  ClippingStore s;
  ASSERT_TRUE(s.loadForBook(path, "Book", "Author", "epub"));
  ASSERT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "Chapter", 0, "quote", uint32_t{1}), ClippingStore::AddResult::Added);
  ASSERT_TRUE(s.prepareSync());
  s.unload();
}
}  // namespace
TEST(ClippingBookMove, CheckedAdmissionFaultsPrecedeIntentAndAllMetadataWrites) {
  for (bool queued : {false, true}) {
    SCOPED_TRACE(queued);
    size_t allocations = 0;
    for (int fault = -1; fault < static_cast<int>(allocations); ++fault) {
      SCOPED_TRACE(fault);
      fake::reset();
      fake::add("/book.epub", "entire physical book");
      seedBookMove("/book.epub");
      if (queued) {
        ClippingStore s;
        ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
        for (unsigned i = 1; i < 34; ++i) {
          ASSERT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "Chapter", 0, "quote", uint32_t{1}),
                    ClippingStore::AddResult::Added);
        }
        ASSERT_TRUE(s.prepareSync());
        for (unsigned i = 0; i < 17; ++i) ASSERT_TRUE(s.removeClippingAt(0));
        s.unload();
      }
      std::map<std::string, std::vector<uint8_t>> original;
      for (const auto& [path, node] : fake::files) original[path] = node->bytes;
      const auto writes = fake::writesByPath;
      const auto bookmarks = BookmarkStore::begins;
      const auto before = fake::checkedAllocationAttempts;
      fake::failureTriggered = false;
      fake::failAlloc = fault;
      const auto result = BookMoveUtils::migrateRenamedBookState("/book.epub", "/new.epub", "", "", "", "epub", true);
      if (fault < 0) {
        ASSERT_EQ(result, BookMoveUtils::RenameMigrationResult::Success);
        allocations = fake::checkedAllocationAttempts - before;
        ASSERT_GT(allocations, 0u);
        RecordProperty(queued ? "queuedCheckedAllocations" : "oneRowCheckedAllocations", static_cast<int>(allocations));
      } else {
        EXPECT_EQ(result, BookMoveUtils::RenameMigrationResult::RolledBack);
        ASSERT_TRUE(fake::failureTriggered);
        EXPECT_FALSE(Storage.exists("/.crosspoint/book-move.pending"));
        EXPECT_FALSE(Storage.exists("/.crosspoint/clippings/move.pending"));
        EXPECT_EQ(fake::writesByPath, writes);
        EXPECT_EQ(BookmarkStore::begins, bookmarks);
        EXPECT_EQ(fake::files.size(), original.size());
        for (const auto& [path, bytes] : original) {
          ASSERT_TRUE(Storage.exists(path.c_str()));
          EXPECT_EQ(fake::files.at(path)->bytes, bytes);
        }
        EXPECT_EQ(fake::liveCheckedAllocations, 0u);
        ASSERT_EQ(BookMoveUtils::migrateRenamedBookState("/book.epub", "/new.epub", "", "", "", "epub", true),
                  BookMoveUtils::RenameMigrationResult::Success);
      }
      ClippingStore loaded;
      ASSERT_TRUE(loaded.loadForBook("/new.epub", "", "", "epub"));
      std::string quote;
      ASSERT_TRUE(loaded.readClippingText(0, quote));
      EXPECT_EQ(quote, "quote");
      loaded.unload();
      EXPECT_EQ(fake::liveCheckedAllocations, 0u);
    }
  }
}
TEST(ClippingBookMove, DestinationHistoryRefusalPrecedesEncompassingIntent) {
  fake::reset();
  fake::add("/book.epub");
  seedBookMove("/book.epub");
  seedBookMove("/new.epub");
  std::map<std::string, std::vector<uint8_t>> original;
  for (const auto& [path, node] : fake::files) original[path] = node->bytes;
  EXPECT_EQ(BookMoveUtils::migrateRenamedBookState("/book.epub", "/new.epub", "", "", "", "epub", true),
            BookMoveUtils::RenameMigrationResult::RolledBack);
  EXPECT_FALSE(Storage.exists("/.crosspoint/book-move.pending"));
  EXPECT_TRUE(Storage.exists("/book.epub"));
  EXPECT_FALSE(Storage.exists("/new.epub"));
  for (const auto& [path, bytes] : original) {
    ASSERT_TRUE(Storage.exists(path.c_str()));
    EXPECT_EQ(fake::files.at(path)->bytes, bytes);
  }
}
TEST(ClippingBookMove, SuccessfulTransactionPreservesQuotesAndUncertainRetainsIntent) {
  for (bool fail : {false, true}) {
    fake::reset();
    fake::add("/book.epub");
    seedBookMove("/book.epub");
    if (fail) fake::failRename = 0;
    auto result = BookMoveUtils::migrateRenamedBookState("/book.epub", "/new.epub", "", "", "", "epub", true);
    if (fail) {
      EXPECT_TRUE(fake::failureTriggered);
      EXPECT_EQ(result, BookMoveUtils::RenameMigrationResult::Uncertain);
      EXPECT_TRUE(Storage.exists("/.crosspoint/book-move.pending"));
      EXPECT_TRUE(Storage.exists("/book.epub"));
      EXPECT_EQ(BookMoveUtils::migrateRenamedBookState("/book.epub", "/new.epub", "", "", "", "epub", true),
                BookMoveUtils::RenameMigrationResult::Uncertain);
    } else {
      ASSERT_EQ(result, BookMoveUtils::RenameMigrationResult::Success);
      EXPECT_FALSE(Storage.exists("/.crosspoint/book-move.pending"));
      EXPECT_TRUE(Storage.exists("/new.epub"));
      ClippingStore s;
      ASSERT_TRUE(s.loadForBook("/new.epub", "", "", "epub"));
      std::string text;
      ASSERT_TRUE(s.readClippingText(0, text));
      EXPECT_EQ(text, "quote");
      std::vector<ClippedBookEntry> books;
      ASSERT_TRUE(ClippingStore::getAllClippedBooks(books));
      ASSERT_EQ(books.size(), 1u);
      EXPECT_EQ(books[0].bookTitle, "Book");
      EXPECT_EQ(books[0].bookAuthor, "Author");
    }
  }
}
