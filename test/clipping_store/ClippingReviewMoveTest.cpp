#include "ReviewMoveFixtures.h"

TEST(ClippingReviewMove, CleanV5TombstonesStillMoveForFilesAndFolders) {
  for (bool folder : {false, true}) {
    reviewMove::cleanTombstones(
        [](const std::string& from, const std::string& to) { return ClippingStore::moveBook(from, to); }, folder);
  }
}

TEST(ClippingReviewMove, SidecarOnlySourcesRefuseBeforeIntentForBothIdentitiesAndFolders) {
  for (bool folder : {false, true}) {
    SCOPED_TRACE(folder);
    for (bool modern : {false, true}) {
      SCOPED_TRACE(modern);
      for (const char* suffix : {".tmp", ".bak", ".rename.bak", ".migrate.bak", ".deleted"}) {
        SCOPED_TRACE(suffix);
        fake::reset();
        const std::string book = folder ? "/folder/book.epub" : "/book.epub";
        const std::string from = folder ? "/folder" : book;
        const std::string to = folder ? "/renamed" : "/new.epub";
        if (folder) Storage.mkdir(from.c_str());
        fake::add(book, "entire physical book");
        const auto owner = reviewMove::legacy(book, 3);
        const auto primary = fake::files.at(owner)->bytes;
        ASSERT_TRUE(Storage.remove(owner.c_str()));
        ASSERT_TRUE(Storage.remove((owner + ".deleted").c_str()));
        const auto root = modern ? std::string("/.crosspoint/clippings/epub_") +
                                       std::to_string(std::hash<std::string>{}(book)) + ".bin"
                                 : owner;
        fake::add(root + suffix, suffix == std::string(".deleted") ? "raw interrupted recovery bytes"
                                                                   : std::string(primary.begin(), primary.end()));
        const auto original = reviewMove::bytes();
        const auto writes = fake::writesByPath;
        EXPECT_FALSE(ClippingStore::moveBook(from, to));
        EXPECT_EQ(reviewMove::bytes(), original);
        EXPECT_EQ(fake::writesByPath, writes);
        EXPECT_FALSE(Storage.exists("/.crosspoint/book-move.pending"));
        EXPECT_FALSE(Storage.exists("/.crosspoint/clippings/move.pending"));
      }
    }
  }
}

TEST(ClippingReviewMove, RawLegacyJournalsRefuseFileAndFolderMovesUntilAcknowledged) {
  for (bool folder : {false, true}) {
    SCOPED_TRACE(folder);
    reviewMove::legacyMove(
        [](const std::string& from, const std::string& to) { return ClippingStore::moveBook(from, to); }, folder);
  }
}
