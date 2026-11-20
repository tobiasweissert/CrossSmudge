TEST(ClippingCaller, ActualRenameRequestsConservativeAndStopsOnUncertain) {
  fake::reset();
  caller::reset();
  fake::add("/book.epub");
  APP_STATE.favoriteSleepImagePath = "/book.epub";
  APP_STATE.favoriteBootImagePath = "/book.epub";
  caller::outcome = BookMoveUtils::RenameMigrationResult::Uncertain;
  FileBrowserActivity browser;
  browser.renameFile("/book.epub", "book.epub", "new", ".epub");
  EXPECT_EQ(caller::migrations, 1);
  EXPECT_TRUE(caller::conservative);
  EXPECT_EQ(APP_STATE.favoriteSleepImagePath, "/book.epub");
  EXPECT_EQ(APP_STATE.favoriteBootImagePath, "/book.epub");
  EXPECT_EQ(caller::errors, 1);
  EXPECT_EQ(caller::invalidations, 0);
  EXPECT_EQ(caller::refreshes, 0);
  EXPECT_EQ(caller::saves, 0);
}
TEST(ClippingCaller, ActualDeleteRefusesRetainedClippingBeforeRemovingBook) {
  for (const char* suffix : {".tmp", ".bak", ".rename.bak", ".migrate.bak"}) {
    SCOPED_TRACE(suffix);
    fake::reset();
    caller::reset();
    fake::add("/book.epub");
    ClippingStore store;
    ASSERT_TRUE(store.loadForBook("/book.epub", "Book", "Author", "epub"));
    ASSERT_EQ(store.addClipping(0, 0, 0, 1, 0, 0, 1, "Chapter", 0, "quote", uint32_t{1}),
              ClippingStore::AddResult::Added);
    store.unload();
    std::string path;
    for (const auto& [candidate, node] : fake::files)
      if (candidate.ends_with(".bin")) path = candidate;
    fake::add(path + suffix, "retained evidence");
    const auto old = fake::files.at(path)->bytes;
    FileBrowserActivity browser;
    browser.deleteConfirmed("/book.epub", {});
    EXPECT_TRUE(Storage.exists("/book.epub"));
    EXPECT_EQ(caller::metadataClears, 0);
    EXPECT_EQ(caller::errors, 1);
    EXPECT_EQ(caller::invalidations, 0);
    EXPECT_EQ(caller::refreshes, 0);
    EXPECT_EQ(fake::files.at(path)->bytes, old);
    EXPECT_TRUE(Storage.exists((path + suffix).c_str()));
  }
}
TEST(ClippingCaller, ActualDeleteUnifiedTextAndGlobalIntentRefuseBeforeBookRemoval) {
  for (const char* book : {"/book.txt", "/book.md"}) {
    fake::reset();
    caller::reset();
    fake::add(book);
    ClippingStore store;
    ASSERT_TRUE(store.loadForBook(book, "Book", "Author", "epub"));
    ASSERT_EQ(store.addClipping(0, 0, 0, 1, 0, 0, 1, "Chapter", 0, "quote", uint32_t{1}),
              ClippingStore::AddResult::Added);
    store.unload();
    std::string path;
    for (const auto& [candidate, node] : fake::files)
      if (candidate.ends_with(".bin")) path = candidate;
    fake::add(path + ".bak", "retained evidence");
    FileBrowserActivity browser;
    browser.deleteConfirmed(book, {});
    EXPECT_TRUE(Storage.exists(book));
    EXPECT_EQ(caller::metadataClears, 0);
    EXPECT_EQ(caller::errors, 1);
  }
  fake::reset();
  caller::reset();
  fake::add("/book.epub");
  fake::add("/.crosspoint/book-move.pending", "uncertain intent");
  FileBrowserActivity browser;
  browser.deleteConfirmed("/book.epub", {});
  EXPECT_TRUE(Storage.exists("/book.epub"));
  EXPECT_EQ(caller::metadataClears, 0);
  EXPECT_EQ(caller::errors, 1);
}
TEST(ClippingCaller, ActualDeleteCleanStoreAndCancelledControl) {
  fake::reset();
  caller::reset();
  fake::add("/book.epub");
  FileBrowserActivity browser;
  browser.deleteConfirmed("/book.epub", {true});
  EXPECT_TRUE(Storage.exists("/book.epub"));
  EXPECT_EQ(caller::metadataClears, 0);
  browser.deleteConfirmed("/book.epub", {});
  EXPECT_FALSE(Storage.exists("/book.epub"));
  EXPECT_EQ(caller::metadataClears, 1);
  EXPECT_EQ(caller::errors, 0);
  EXPECT_EQ(caller::refreshes, 1);
}
