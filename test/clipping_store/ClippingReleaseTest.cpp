#include <ClippingStore.h>
#include <HalStorage.h>
#include <Serialization.h>
#include <gtest/gtest.h>

namespace {
std::string releasePath() {
  for (const auto& [path, node] : fake::files)
    if (path.ends_with(".bin")) return path;
  return "";
}
void emptyV4(const std::string& path) {
  HalFile f = Storage.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  ASSERT_TRUE(serialization::tryWritePod(f, uint8_t{4}));
  ASSERT_TRUE(serialization::tryWritePod(f, uint16_t{0}));
  ASSERT_TRUE(serialization::tryWriteString(f, "Stored title"));
  ASSERT_TRUE(serialization::tryWriteString(f, "Stored author"));
  ASSERT_TRUE(serialization::tryWriteString(f, "/book.epub"));
  ASSERT_TRUE(f.close());
}
std::string seedRelease(ClippingStore& s) {
  EXPECT_TRUE(s.loadForBook("/book.epub", "Stored title", "Stored author", "epub"));
  EXPECT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "chapter", 0, "quote", uint32_t{1}), ClippingStore::AddResult::Added);
  return releasePath();
}
}  // namespace
TEST(ClippingRelease, FixedModernS3HistoricalCardinalitiesAreNotHidden) {
  for (uint16_t count : {uint16_t{257}, uint16_t{1024}, uint16_t{1025}}) {
    SCOPED_TRACE(count);
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    s.unload();
    HalFile f = Storage.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    ASSERT_TRUE(serialization::tryWritePod(f, uint8_t{4}));
    ASSERT_TRUE(serialization::tryWritePod(f, count));
    ASSERT_TRUE(serialization::tryWriteString(f, "History"));
    ASSERT_TRUE(serialization::tryWriteString(f, "Author"));
    ASSERT_TRUE(serialization::tryWriteString(f, "/book.epub"));
    for (uint16_t i = 0; i < count; ++i) {
      for (uint16_t value :
           {i, uint16_t{0}, uint16_t{0}, uint16_t{1}, uint16_t{0}, uint16_t{0}, uint16_t{1}, uint16_t{2}})
        ASSERT_TRUE(serialization::tryWritePod(f, value));
      ASSERT_TRUE(serialization::tryWritePod(f, uint32_t{123}));
      ASSERT_TRUE(serialization::tryWritePod(f, uint32_t{456}));
      ASSERT_TRUE(serialization::tryWritePod(f, uint32_t{100}));
      ASSERT_TRUE(serialization::tryWritePod(f, uint32_t{200}));
      ASSERT_TRUE(serialization::tryWritePod(f, uint64_t{0x123456789abcdef0ULL}));
      ASSERT_TRUE(serialization::tryWritePod(f, uint8_t{0}));
      char id[65];
      memset(id, 'x', 64);
      id[64] = 0;
      snprintf(id, 9, "%08u", i);
      id[8] = 'x';
      ASSERT_EQ(f.write(id, sizeof(id)), sizeof(id));
      char chapter[48] = "Chapter";
      ASSERT_EQ(f.write(chapter, sizeof(chapter)), sizeof(chapter));
      ASSERT_TRUE(serialization::tryWritePod(f, uint16_t{7}));
      ASSERT_EQ(f.write("history", 7), 7u);
    }
    ASSERT_TRUE(f.close());
    const auto original = fake::files.at(path)->bytes;
    const bool loaded = s.loadForBook("/book.epub", "", "", "epub");
#ifdef CONFIG_IDF_TARGET_ESP32S3
    EXPECT_EQ(loaded, count <= 1024);
#else
    EXPECT_FALSE(loaded);
#endif
    EXPECT_EQ(fake::files.at(path)->bytes, original);
    if (loaded) {
      ASSERT_EQ(s.clippingCount(), count);
      const auto* row = s.clippingAt(count - 1);
      ASSERT_NE(row, nullptr);
      EXPECT_EQ(strlen(row->id), 64u);
      EXPECT_EQ(row->syncRevision, 0x123456789abcdef0ULL);
      EXPECT_EQ(row->tableSelection, UINT16_MAX);
      EXPECT_EQ(row->startOffset, 100u);
      EXPECT_EQ(row->endOffset, 200u);
      std::string text;
      ASSERT_TRUE(s.readClippingText(count - 1, text));
      EXPECT_EQ(text, "history");
      // Import is not enough: exercise the checked v5 transaction at the fixed
      // historical cardinality, then reacquire every identity/text after load.
      Clipping update = *row;
      ++update.syncRevision;
      ASSERT_TRUE(s.applyRemote(update, "updated history", false));
      s.unload();
      ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
      ASSERT_EQ(s.clippingCount(), count);
      for (uint16_t i = 0; i < count; ++i) {
        ASSERT_NE(s.clippingAt(i), nullptr);
        EXPECT_EQ(s.clippingAt(i)->spineIndex, i);
        EXPECT_EQ(strlen(s.clippingAt(i)->id), 64u);
        EXPECT_EQ(s.clippingAt(i)->tableSelection, UINT16_MAX);
        EXPECT_EQ(s.clippingAt(i)->startOffset, 100u);
        EXPECT_EQ(s.clippingAt(i)->endOffset, 200u);
        ASSERT_TRUE(s.readClippingText(i, text));
        EXPECT_EQ(text, i == count - 1 ? "updated history" : "history");
      }
    }
  }
}
TEST(ClippingRelease, PublishedVerificationReadFailureQuarantinesOldOffsets) {
  unsigned readCount = 0;
  for (bool inject : {false, true}) {
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    ASSERT_EQ(s.addClipping(1, 0, 0, 1, 0, 0, 1, "chapter", 0, "second quote", uint32_t{1}),
              ClippingStore::AddResult::Added);
    ASSERT_TRUE(s.prepareSync());
    const Clipping old = *s.clippingAt(1);
    fake::reads = 0;
    if (inject) fake::failRead = static_cast<int>(readCount) - 1;
    const bool removed = s.removeClippingAt(0);
    if (!inject) {
      ASSERT_TRUE(removed);
      readCount = fake::reads;
      ASSERT_GT(readCount, 1u);
      continue;
    }
    ASSERT_TRUE(fake::failureTriggered);
    ASSERT_FALSE(removed);
    ASSERT_TRUE(Storage.exists(path.c_str()));
    ASSERT_TRUE(Storage.exists((path + ".bak").c_str()));
    EXPECT_EQ(s.clippingAt(0), nullptr);
    EXPECT_EQ(s.clippingCount(), 0u);
    EXPECT_TRUE(s.getClippings().empty());
    std::string text;
    EXPECT_FALSE(s.readClippingText(old, text));
    EXPECT_FALSE(s.readClippingText(0, text));
    EXPECT_FALSE(s.readClippingPreview(0, text));
    EXPECT_FALSE(s.prepareSync());
    EXPECT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "chapter", 0, "retry", uint32_t{1}),
              ClippingStore::AddResult::SaveFailed);
    const auto committed = fake::files.at(path)->bytes;
    s.unload();
    EXPECT_FALSE(s.loadForBook("/book.epub", "", "", "epub"));
    EXPECT_EQ(fake::files.at(path)->bytes, committed);
  }
}
TEST(ClippingRelease, VerifiedPublicationRemainsAuthoritativeAfterNegativeBackupRemoval) {
  for (bool actuallyRemoved : {false, true}) {
    SCOPED_TRACE(actuallyRemoved);
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    ASSERT_EQ(s.addClipping(1, 0, 0, 1, 0, 0, 1, "chapter", 0, "second different length quote", uint32_t{1}),
              ClippingStore::AddResult::Added);
    ASSERT_TRUE(s.prepareSync());
    const std::string deletedId = s.clippingAt(0)->id;
    fake::failRemovePath = path + ".bak";
    fake::removeDespiteFailure = actuallyRemoved;
    ASSERT_TRUE(s.removeClippingAt(0));
    ASSERT_TRUE(fake::failureTriggered);
    ASSERT_EQ(s.clippingCount(), 1u);
    std::string text;
    ASSERT_TRUE(s.readClippingText(0, text));
    EXPECT_EQ(text, "second different length quote");
    uint32_t offset = 0;
    char id[65];
    ASSERT_TRUE(s.nextDeletion(offset, id));
    EXPECT_STREQ(id, deletedId.c_str());
    EXPECT_EQ(Storage.exists((path + ".bak").c_str()), !actuallyRemoved);
    const auto committed = fake::files.at(path)->bytes;
    const auto result = s.addClipping(1, 0, 0, 1, 0, 0, 1, "chapter", 0, "retry quote", uint32_t{1});
    EXPECT_EQ(result, actuallyRemoved ? ClippingStore::AddResult::Added : ClippingStore::AddResult::SaveFailed);
    if (!actuallyRemoved) {
      EXPECT_EQ(fake::files.at(path)->bytes, committed);
    }
    ASSERT_TRUE(s.readClippingText(0, text));
    EXPECT_EQ(text, "second different length quote");
    s.unload();
    EXPECT_EQ(s.loadForBook("/book.epub", "", "", "epub"), actuallyRemoved);
    if (actuallyRemoved) {
      ASSERT_TRUE(s.readClippingText(0, text));
      EXPECT_EQ(text, "second different length quote");
      ASSERT_TRUE(s.readClippingText(1, text));
      EXPECT_EQ(text, "retry quote");
    }
  }
}
TEST(ClippingRelease, LegacyInterruptedPrefixIsPreservedUntilCheckedV5Commit) {
  for (size_t tail = 1; tail < 65; ++tail) {
    SCOPED_TRACE(tail);
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    s.unload();
    emptyV4(path);
    const auto original = fake::files.at(path)->bytes;
    std::string journal(65, '\0');
    journal.replace(0, 7, "deleted");
    journal.append(tail, 'x');
    fake::add(path + ".deleted", journal);
    ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
    EXPECT_EQ(fake::files.at(path)->bytes, original);
    EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, std::vector<uint8_t>(journal.begin(), journal.end()));
    ASSERT_TRUE(s.prepareSync());
    EXPECT_EQ(fake::files.at(path)->bytes[0], 5);
    EXPECT_TRUE(Storage.exists((path + ".deleted").c_str()));
    s.unload();
    ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
    uint32_t offset = 0;
    char id[65];
    ASSERT_TRUE(s.nextDeletion(offset, id));
    EXPECT_STREQ(id, "deleted");
    ASSERT_TRUE(s.nextDeletion(offset, id));
    EXPECT_EQ(id[0], 0);
    ASSERT_TRUE(s.finishDeletions());
    EXPECT_FALSE(Storage.exists((path + ".deleted").c_str()));
  }
  fake::reset();
  ClippingStore s;
  const auto path = seedRelease(s);
  s.unload();
  emptyV4(path);
  std::string malformed(65, 'x');
  fake::add(path + ".deleted", malformed);
  EXPECT_FALSE(s.loadForBook("/book.epub", "", "", "epub"));
  EXPECT_FALSE(s.prepareSync());
  EXPECT_EQ(fake::files.at(path)->bytes[0], 4);
  EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, std::vector<uint8_t>(malformed.begin(), malformed.end()));
}
TEST(ClippingRelease, DestinationCollisionDoesNotCreateMoveIntentOrPoisonDelete) {
  fake::reset();
  fake::add("/book.epub");
  ClippingStore s;
  const auto path = seedRelease(s);
  s.unload();
  ClippingStore destination;
  ASSERT_TRUE(destination.loadForBook("/new.epub", "Destination", "Author", "epub"));
  ASSERT_EQ(destination.addClipping(0, 0, 0, 1, 0, 0, 1, "chapter", 0, "destination history", uint32_t{1}),
            ClippingStore::AddResult::Added);
  destination.unload();
  std::string destinationPath;
  for (const auto& [candidate, node] : fake::files)
    if (candidate.ends_with(".bin") && candidate != path) destinationPath = candidate;
  ASSERT_FALSE(destinationPath.empty());
  const auto original = fake::files.at(path)->bytes;
  EXPECT_FALSE(ClippingStore::moveBook("/book.epub", "/new.epub"));
  EXPECT_FALSE(Storage.exists("/.crosspoint/clippings/move.pending"));
  EXPECT_TRUE(Storage.exists("/book.epub"));
  EXPECT_FALSE(Storage.exists("/new.epub"));
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  EXPECT_TRUE(ClippingStore::deleteForFilePath("/book.epub", "epub"));
  EXPECT_TRUE(Storage.exists(destinationPath.c_str()));
}
TEST(ClippingRelease, HistoricalAliasConflictsAndRetainedRootsAreReadOnly) {
  for (const char* suffix : {"", ".deleted", ".tmp", ".bak", ".rename.bak", ".migrate.bak"}) {
    SCOPED_TRACE(suffix);
    fake::reset();
    ClippingStore s;
    const auto owner = seedRelease(s);
    s.unload();
    const auto modern =
        std::string("/.crosspoint/clippings/epub_") + std::to_string(std::hash<std::string>{}("/book.epub")) + ".bin";
    ASSERT_NE(owner, modern);
    fake::add(modern + suffix, "independent alias bytes");
    const auto ownerBytes = fake::files.at(owner)->bytes;
    const auto aliasBytes = fake::files.at(modern + suffix)->bytes;
    EXPECT_FALSE(s.loadForBook("/book.epub", "", "", "epub"));
    EXPECT_FALSE(ClippingStore::deleteForFilePath("/book.epub", "epub"));
    ClippingStore::RenameMigration migration;
    EXPECT_FALSE(ClippingStore::beginRenameMigration("/book.epub", "/new.epub", "", "", "epub", migration, true));
    ASSERT_TRUE(Storage.exists(owner.c_str()));
    EXPECT_EQ(fake::files.at(owner)->bytes, ownerBytes);
    EXPECT_EQ(fake::files.at(modern + suffix)->bytes, aliasBytes);
  }
}
TEST(ClippingRelease, DestinationModernAliasAndAbsentSourceRootsRefuseRename) {
  for (const char* suffix : {"", ".deleted", ".tmp", ".bak", ".rename.bak", ".migrate.bak"}) {
    SCOPED_TRACE(suffix);
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    s.unload();
    const auto modern =
        std::string("/.crosspoint/clippings/epub_") + std::to_string(std::hash<std::string>{}("/new.epub")) + ".bin";
    fake::add(modern + suffix, "destination bytes");
    const auto original = fake::files.at(path)->bytes;
    ClippingStore::RenameMigration migration;
    EXPECT_FALSE(ClippingStore::beginRenameMigration("/book.epub", "/new.epub", "", "", "epub", migration, true));
    EXPECT_FALSE(migration.active);
    EXPECT_EQ(fake::files.at(path)->bytes, original);
    EXPECT_EQ(fake::files.at(modern + suffix)->bytes, std::vector<uint8_t>({'d', 'e', 's', 't', 'i', 'n', 'a', 't', 'i',
                                                                            'o', 'n', ' ', 'b', 'y', 't', 'e', 's'}));
  }
  for (const char* suffix : {".deleted", ".tmp", ".bak", ".rename.bak", ".migrate.bak"}) {
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    s.unload();
    ASSERT_TRUE(Storage.remove(path.c_str()));
    fake::add(path + suffix, "retained source");
    ClippingStore::RenameMigration migration;
    EXPECT_FALSE(ClippingStore::beginRenameMigration("/book.epub", "/new.epub", "", "", "epub", migration, true));
    EXPECT_TRUE(Storage.exists((path + suffix).c_str()));
  }
}
TEST(ClippingRelease, EveryDeletionSnapshotWriteFailureAllowsCoherentRetry) {
  // One surviving row forces the writer through tombstones, fields and text.
  unsigned boundaries = 0;
  for (int fault = -1; fault < static_cast<int>(boundaries) || fault == -1; ++fault) {
    SCOPED_TRACE(fault);
    fake::reset();
    ClippingStore s;
    const auto path = seedRelease(s);
    ASSERT_EQ(s.addClipping(1, 0, 0, 1, 0, 0, 1, "chapter", 0, "surviving quote", uint32_t{1}),
              ClippingStore::AddResult::Added);
    ASSERT_TRUE(s.prepareSync());
    const std::string deletedId = s.clippingAt(0)->id;
    const auto original = fake::files.at(path)->bytes;
    fake::writesByPath.clear();
    fake::failWrite = fault;
    if (fault == -1) {
      ASSERT_TRUE(s.removeClippingAt(0));
      boundaries = fake::writesByPath[path + ".tmp"];
      ASSERT_GT(boundaries, 20u);
      continue;
    }
    ASSERT_FALSE(s.removeClippingAt(0));
    ASSERT_TRUE(fake::failureTriggered);
    EXPECT_EQ(fake::files.at(path)->bytes, original);
    EXPECT_FALSE(Storage.exists((path + ".tmp").c_str()));
    ASSERT_EQ(s.clippingCount(), 2u);
    std::string text;
    ASSERT_TRUE(s.readClippingText(0, text));
    EXPECT_EQ(text, "quote");
    ASSERT_TRUE(s.readClippingText(1, text));
    EXPECT_EQ(text, "surviving quote");
    uint32_t offset = 0;
    char id[65];
    ASSERT_TRUE(s.nextDeletion(offset, id));
    EXPECT_EQ(id[0], 0);
    ASSERT_TRUE(s.removeClippingAt(0));
    s.unload();
    ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
    ASSERT_EQ(s.clippingCount(), 1u);
    ASSERT_TRUE(s.readClippingText(0, text));
    EXPECT_EQ(text, "surviving quote");
    offset = 0;
    ASSERT_TRUE(s.nextDeletion(offset, id));
    EXPECT_STREQ(id, deletedId.c_str());
  }
}
TEST(ClippingRelease, EmptyMetadataSyncPreservesStoredMetadataAndCallerWins) {
  fake::reset();
  fake::add("/book.epub");
  ClippingStore s;
  seedRelease(s);
  s.unload();
  ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
  ASSERT_TRUE(s.prepareSync());
  ASSERT_TRUE(s.finishUploads());
  std::vector<ClippedBookEntry> books;
  ASSERT_TRUE(ClippingStore::getAllClippedBooks(books));
  ASSERT_EQ(books.size(), 1u);
  EXPECT_EQ(books[0].bookTitle, "Stored title");
  EXPECT_EQ(books[0].bookAuthor, "Stored author");
  s.unload();
  ASSERT_TRUE(s.loadForBook("/book.epub", "Caller title", "Caller author", "epub"));
  ASSERT_TRUE(s.removeClippingAt(0));
  ASSERT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "chapter", 0, "quote", uint32_t{1}), ClippingStore::AddResult::Added);
  books.clear();
  ASSERT_TRUE(ClippingStore::getAllClippedBooks(books));
  ASSERT_EQ(books.size(), 1u);
  EXPECT_EQ(books[0].bookTitle, "Caller title");
  EXPECT_EQ(books[0].bookAuthor, "Caller author");
}
TEST(ClippingRelease, EmptyV4EquivalentInterpretationsRemainAvailable) {
  fake::reset();
  ClippingStore s;
  const auto path = seedRelease(s);
  s.unload();
  emptyV4(path);
  const auto original = fake::files.at(path)->bytes;
  ASSERT_TRUE(s.loadForBook("/book.epub", "", "", "epub"));
  EXPECT_EQ(s.clippingCount(), 0u);
  EXPECT_EQ(fake::files.at(path)->bytes, original);
  EXPECT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "chapter", 0, "new quote", uint32_t{1}),
            ClippingStore::AddResult::Added);
}
