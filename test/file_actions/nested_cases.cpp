// Real production nested methods; only storage/path identities and unrelated
// reference services are adapters. Fixtures are written by the real codecs.
int main() {
  std::cout << std::unitbuf;
  const std::string old = "/Books/A.epub", next = "/Target/A.epub";
  const auto src = currentStoreFilePathForBook(old, "epub"), dst = currentStoreFilePathForBook(next, "epub");
  readbackFaultPath = dst;
  auto fixture = [&](const std::string& path, int spine = 1) {
    BookmarkStore store;
    store.bookFilePath = path;
    store.bookTitle = "Title";
    store.bookAuthor = "Author";
    store.storeFilePath = currentStoreFilePathForBook(path, "epub");
    Bookmark bm{};
    bm.spineIndex = spine;
    bm.progress = 0.5f;
    bm.timestamp = 123;
    strcpy(bm.chapterTitle, "Chapter");
    strcpy(bm.snippet, "Bookmark text");
    store.bookmarks = {bm};
    assert(store.writeToFile());
    return disk.at(store.storeFilePath);
  };
  const auto sourceBytes = fixture(old), destBytes = fixture(next, 2);
  auto reset = [&](bool destination = false) {
    disk = {{old, "BOOK_FILE"}, {src, sourceBytes}};
    if (destination) disk[dst] = destBytes;
    failBackup = shortWrite = failSync = failRead = corruptRead = mutateRenameFailure = false;
    failRenameTo.clear();
    failWriteAt = writeCalls = shortWrites = 0;
  };
  auto logicalPreservedWithIntent = [&](const NestedDisk& before) {
    // Current BookMove acknowledges encompassing intent before Bookmark reads.
    // Exclude only this exact journal, and verify its complete retained bytes.
    const std::string marker = "/.crosspoint/book-move.pending";
    return disk.count(marker) && disk.at(marker) == "CPBOOKMOVE1\n" + old + "\n" + next + "\n" &&
           disk.equalsExcept(before, marker);
  };
  auto move = [&](bool conservative = true) {
    return BookMoveUtils::migrateRenamedBookState(old, next, "", "A", "Author", "epub", conservative);
  };
  reset(true);
  failBackup = true;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
  assert(disk.at(old) == "BOOK_FILE" && disk.at(src) == sourceBytes && disk.at(dst) == destBytes);
  std::cout << "PASS nested backup false/no-mutation preserves both originals\n";
  reset(true);
  failBackup = true;
  assert(move(false) == BookMoveUtils::RenameMigrationResult::RolledBack);
  assert(disk.at(dst) == destBytes);
  std::cout << "PASS nested legacy rollback never deletes untouched destination\n";
  reset();
  shortWrite = true;
  assert(move() != BookMoveUtils::RenameMigrationResult::Success);
  assert(disk.count(old) && disk.at(src) == sourceBytes);
  std::cout << "PASS nested zero-byte writes retain complete source\n";
  reset();
  assert(move() == BookMoveUtils::RenameMigrationResult::Success);
  const int writes = writeCalls;
  assert(!disk.count(src) && disk.at(next) == "BOOK_FILE");
  BookmarkStore reader;
  reader.bookFilePath = next;
  std::vector<Bookmark> loaded;
  bool rewrite = false;
  assert(reader.readFromFile(dst, loaded, rewrite) && loaded.size() == 1 && loaded[0].timestamp == 123);
  for (int i = 1; i <= writes; ++i) {
    reset();
    failWriteAt = i;
    assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
    assert(disk.at(src) == sourceBytes && disk.at(old) == "BOOK_FILE" && !disk.count(next));
    std::cout << "CASE bookmark-write/" << i << "\n";
  }
  std::cout << "PASS nested each header/string/POD/raw-array write failure (" << writes << ")\n";
  reset(true);
  failSync = true;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
  assert(disk.at(src) == sourceBytes && disk.at(dst + ".rename.bak") == destBytes && disk.count(old));
  std::cout << "PASS nested sync failure retains source and destination backup\n";
  reset();
  failRead = true;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
  assert(disk.at(src) == sourceBytes && disk.count(old));
  std::cout << "PASS nested readback failure retains complete source\n";
  reset();
  corruptRead = true;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain && disk.at(src) == sourceBytes && disk.count(old));
  std::cout << "PASS nested same-size readback corruption retains source\n";
  reset(true);
  failRenameTo = dst + ".rename.bak";
  mutateRenameFailure = true;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
  assert(disk.at(dst) == destBytes && disk.at(dst + ".rename.bak") == destBytes && disk.at(src) == sourceBytes);
  std::cout << "PASS nested unacknowledged backup preserves all candidates without rollback\n";
  reset(true);
  assert(move() == BookMoveUtils::RenameMigrationResult::Success);
  loaded.clear();
  assert(reader.readFromFile(dst, loaded, rewrite) && loaded.size() == 2);
  assert(!disk.count(src) && !disk.count(dst + ".rename.bak"));
  std::cout << "PASS nested successful merge verifies full records before commit\n";
  reset(true);
  BookmarkStore::RenameMigration migration;
  assert(BookmarkStore::beginRenameMigration(old, next, "A", "Author", "epub", migration, true));
  assert(BookmarkStore::rollbackRenameMigration(migration));
  assert(disk.at(src) == sourceBytes && disk.at(dst) == destBytes);
  std::cout << "PASS nested acknowledged rollback restores both original records\n";
  reset();
  shortWrite = true;
  assert(!BookmarkStore::beginRenameMigration(old, next, "A", "Author", "epub", migration, true));
  const auto uncertainSnapshot = disk;
  assert(!BookmarkStore::commitRenameMigration(migration) && !BookmarkStore::rollbackRenameMigration(migration));
  assert(disk == uncertainSnapshot && disk.at(src) == sourceBytes);
  std::cout << "PASS nested uncertain transaction rejects commit and rollback without mutation\n";
  reset(true);
  disk[dst + ".rename.bak"] = "RETAINED";
  const auto retained = disk;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain && logicalPreservedWithIntent(retained));
  std::cout << "PASS nested retained recovery candidates are never rotated\n";
  reset();
  BookmarkStore oversized;
  oversized.storeFilePath = dst;
  oversized.bookmarks.resize(MAX_BOOKMARKS + 1);
  disk[dst] = "SENTINEL";
  assert(!oversized.writeToFile() && disk.at(dst) == "SENTINEL");
  std::cout << "PASS nested record count bounded before truncate\n";
  reset();
  BookmarkStore full;
  full.bookFilePath = next;
  full.storeFilePath = dst;
  for (size_t i = 0; i < MAX_BOOKMARKS; ++i) {
    Bookmark bm{};
    bm.spineIndex = i + 2;
    full.bookmarks.push_back(bm);
  }
  assert(full.writeToFile());
  const auto fullBytes = disk.at(dst);
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain && disk.at(dst) == fullBytes &&
         disk.at(src) == sourceBytes);
  std::cout << "PASS nested overflowing merge refuses before destination mutation\n";

  // Production-reader regression for the documented timestamp -1, before broad boundaries.
  reset(true);
  inputFaultPath = src;
  inputFaultPosition = 3 + (4 + 5) + (4 + 6) + (4 + old.size()) + 2 + 4;
  inputFault = true;
  inputFaultReturn = -1;
  inputFaultHits = inputReadsAfterFault = mutations = 0;
  const auto readFailureOriginals = disk;
  const auto readFailureResult = move();
  std::cout << "timestamp -1 result=" << static_cast<int>(readFailureResult) << " source_exists=" << disk.count(src)
            << " mutations=" << mutations << std::endl;
  assert(readFailureResult != BookMoveUtils::RenameMigrationResult::Success);
  assert(logicalPreservedWithIntent(readFailureOriginals) && inputFaultHits == 1 && inputReadsAfterFault == 0 &&
         mutations == 1);
  inputFaultPath.clear();
  inputFaultHits = 0;
  std::cout << "PASS nested negative timestamp read refuses before logical mutation with intent retained\n";

  // Same real writer bytes expressed in each supported historical record layout.
  auto versionBytes = [&](std::string bytes, int version) {
    uint32_t pathLength = 0;
    memcpy(&pathLength, bytes.data() + 22, 4);
    const size_t record = 26 + pathLength;
    if (version < 5) bytes.erase(record + 2 + 4 + 4 + BOOKMARK_CHAPTER_TITLE_MAX + 2, BOOKMARK_SNIPPET_MAX);
    if (version < 4) bytes.erase(record + 2 + 4 + 4 + BOOKMARK_CHAPTER_TITLE_MAX, 2);
    if (version == 2) bytes.erase(2, 1);
    bytes[0] = static_cast<char>(version);
    return bytes;
  };
  int readFaultCases = 0;
  for (int version = 2; version <= 5; ++version) {
    const auto sourceVersion = versionBytes(sourceBytes, version), destVersion = versionBytes(destBytes, version);
    std::vector<size_t> sizes = {
        1, version == 2 ? size_t(1) : size_t(2), 4, 5, 4, 6, 4, old.size(), 2, 4, 4, BOOKMARK_CHAPTER_TITLE_MAX};
    if (version >= 4) sizes.push_back(2);
    if (version >= 5) sizes.push_back(BOOKMARK_SNIPPET_MAX);
    for (bool destination : {false, true})
      for (bool legacy : {false, true})
        for (int failure : {-1, 0}) {
          sizes[7] = destination ? next.size() : old.size();
          size_t position = 0;
          for (size_t size : sizes) {
            reset(true);
            disk[src] = sourceVersion;
            disk[dst] = destVersion;
            const std::string current = destination ? dst : src;
            inputFaultPath = legacy ? legacyStoreFilePathForBook(destination ? next : old, "epub") : current;
            if (legacy) {
              disk[inputFaultPath] = disk.at(current);
              disk.erase(current);
            }
            inputFaultPosition = position;
            inputFaultReturn = failure;
            inputFault = true;
            inputFaultHits = inputReadsAfterFault = mutations = 0;
            const auto originals = disk;
            assert(move() != BookMoveUtils::RenameMigrationResult::Success);
            assert(inputFaultHits == 1 && inputReadsAfterFault == 0 && mutations == 1 &&
                   logicalPreservedWithIntent(originals));
            std::cout << "CASE bookmark-read/v" << version << "/destination=" << destination << "/legacy=" << legacy
                      << "/failure=" << failure << "/position=" << position << "\n";
            ++readFaultCases;
            position += size;
          }
        }
    inputFaultPath.clear();
    inputFaultHits = 0;
    reset(true);
    disk[src] = sourceVersion;
    disk[dst] = destVersion;
    assert(move() == BookMoveUtils::RenameMigrationResult::Success);
    loaded.clear();
    assert(reader.readFromFile(dst, loaded, rewrite) && loaded.size() == 2);
    assert(loaded[0].timestamp == 123 && loaded[1].timestamp == 123);
    assert(std::string(loaded[0].chapterTitle) == "Chapter");
    assert(loaded[0].paragraphIndex == UINT16_MAX);
    assert(std::string(loaded[0].snippet) == (version == 5 ? "Bookmark text" : ""));
    // Legacy default-false callers retain healthy migration semantics.
    reset(true);
    disk[src] = sourceVersion;
    disk[dst] = destVersion;
    assert(move(false) == BookMoveUtils::RenameMigrationResult::Success);
    std::cout << "CASE bookmark-healthy/v" << version << "\n";
  }
  std::cout << "PASS nested source/destination current/legacy each short/negative read boundary v2-v5 ("
            << readFaultCases << ")\n";
  std::cout << "PASS nested v2-v5 healthy Move and ordinary legacy caller controls\n";
  // Truncation and oversized length must not publish even one partial record.
  inputFaultPath.clear();
  inputFaultHits = 0;
  for (size_t length = 0; length < sourceBytes.size(); ++length) {
    reset(true);
    disk[src] = sourceBytes.substr(0, length);
    const auto originals = disk;
    BookmarkStore sourceReader;
    sourceReader.bookFilePath = old;
    std::vector<Bookmark> sentinel(1);
    sentinel[0].timestamp = 987;
    bool needsRewrite = true;
    assert(!sourceReader.readFromFile(src, sentinel, needsRewrite));
    assert(sentinel.size() == 1 && sentinel[0].timestamp == 987 && !needsRewrite);
    assert(move() != BookMoveUtils::RenameMigrationResult::Success && logicalPreservedWithIntent(originals));
    std::cout << "CASE bookmark-truncate/" << length << "\n";
  }
  for (size_t offset : {size_t(3), size_t(12), size_t(22)}) {
    reset(true);
    disk[src].replace(offset, 4, 4, '\xff');
    const auto originals = disk;
    assert(move() != BookMoveUtils::RenameMigrationResult::Success && logicalPreservedWithIntent(originals));
    std::cout << "CASE bookmark-length/" << offset << "\n";
  }
  reset(true);
  disk[src] += sourceBytes.substr(3 + (4 + 5) + (4 + 6) + (4 + old.size()), 2);
  disk[src][1] = 2;
  const auto partialOriginals = disk;
  assert(move() != BookMoveUtils::RenameMigrationResult::Success && logicalPreservedWithIntent(partialOriginals));
  std::cout << "PASS nested truncated strings/records and oversized lengths refuse without publication\n";
  // Use the real clipping writer and preparation, not beginOk adapters.
  reset();
  ClippingStore clipping;
  clipping.bookFilePath = old;
  clipping.storeFilePath = storeFilePathForBook(old, "epub");
  assert(clipping.clippings.push_back(Clipping{}));
  const std::string text = "Saved selection";
  assert(clipping.writeToFile(&text, 0));
  const auto clipSource = disk.at(clipping.storeFilePath);
  const auto clipDest = storeFilePathForBook(next, "epub");
  disk[clipDest] = "DESTINATION_CLIPPINGS";
  assert(move() == BookMoveUtils::RenameMigrationResult::RolledBack);
  assert(disk.at(clipping.storeFilePath) == clipSource && disk.at(clipDest) == "DESTINATION_CLIPPINGS" &&
         disk.count(old));
  std::cout << "PASS nested manual Move refuses retained clipping conflict\n";
  disk.erase(clipDest);
  failRenameTo = clipDest;
  mutateRenameFailure = true;
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
  assert(disk.at(clipping.storeFilePath) == clipSource && disk.count(clipDest + ".tmp") && disk.count(clipDest) &&
         disk.at(src) == sourceBytes);
  std::cout << "PASS nested clipping unacknowledged publication preserves candidates\n";
  // A fresh healthy fixture, not a retry over uncertain candidates.
  reset();
  disk[clipping.storeFilePath] = clipSource;
  disk.erase(clipDest);
  assert(move() == BookMoveUtils::RenameMigrationResult::Success);
  assert(!disk.count(clipping.storeFilePath) && disk.count(clipDest));
  std::cout << "PASS nested real clipping preparation publication and commit\n";
  ClippingStore reopened;
  assert(reopened.loadForBook(next, "", "", "epub") && reopened.clippingCount() == 1);
  std::string reopenedText;
  assert(reopened.readClippingText(size_t{0}, reopenedText) && reopenedText == text);
  assert(static_cast<unsigned char>(disk.at(clipDest)[0]) == 5);
  std::cout << "PASS nested actual v5 reopened path and text\n";

  // Count every write in the composed intent + bookmark + checked-v5 writer.
  const int composedWrites = writeCalls;
  for (int i = 1; i <= composedWrites; ++i) {
    reset();
    disk[clipping.storeFilePath] = clipSource;
    failWriteAt = i;
    assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
    assert(shortWrites == 1 && disk.at(src) == sourceBytes && disk.at(old) == "BOOK_FILE" &&
           disk.at(clipping.storeFilePath) == clipSource && !disk.count(next));
    std::cout << "CASE composed-write/" << i << "\n";
  }
  for (int failure : {-1, 0}) {
    reset();
    disk[clipping.storeFilePath] = clipSource;
    inputFaultPath = clipping.storeFilePath;
    inputFaultPosition = 0;
    inputFaultReturn = failure;
    inputFault = true;
    inputFaultHits = inputReadsAfterFault = mutations = 0;
    const auto before = disk;
    assert(move() == BookMoveUtils::RenameMigrationResult::RolledBack);
    assert(disk == before && inputFaultHits == 1 && inputReadsAfterFault == 0 && mutations == 0);
    inputFaultPath.clear();
    inputFaultHits = 0;
    std::cout << "CASE clipping-preparation-read/" << failure << "\n";
  }
  reset();
  disk[clipping.storeFilePath] = clipSource;
  fake::failClosePath = clipDest + ".tmp";
  assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain && fake::failureTriggered);
  assert(disk.at(clipping.storeFilePath) == clipSource && disk.at(src) == sourceBytes &&
         disk.count(clipDest + ".tmp") && disk.count(old) && !disk.count(next));
  std::cout << "PASS nested negative clipping close retains unpublished candidate\n";

  reset();
  disk[clipping.storeFilePath] = clipSource;
  ClippingStore saveFailure;
  assert(saveFailure.loadForBook(old, "", "", "epub"));
  fake::shortWrite = 0;
  assert(!saveFailure.removeClippingAt(0) && saveFailure.clippingCount() == 1);
  assert(disk.at(clipping.storeFilePath) == clipSource);
  ClippingStore afterSaveFailure;
  assert(afterSaveFailure.loadForBook(old, "", "", "epub"));
  assert(afterSaveFailure.readClippingText(size_t{0}, reopenedText) && reopenedText == text);
  std::cout << "PASS nested failed real clipping save preserves record and reopened bytes\n";

  for (bool modern : {false, true})
    for (const char* suffix : {"", ".deleted", ".bak", ".tmp", ".rename.bak", ".migrate.bak"}) {
      reset();
      disk[clipping.storeFilePath] = clipSource;
      disk[(modern ? modernStorePathForBook(next, "epub") : clipDest) + suffix] = "RETAINED_HISTORY";
      const auto before = disk;
      assert(move() == BookMoveUtils::RenameMigrationResult::RolledBack && disk == before);
      std::cout << "CASE clipping-destination-root/modern=" << modern << "/suffix=" << suffix << "\n";
    }
  // Genuine historical v4 wire: eight u16 anchors, timestamp/layout, owner
  // table u16 OR modern sync fields, then fixed title and length-delimited text.
  // No v5 bytes are relabelled. Both historical families migrate through real TU.
  for (bool modern : {false, true}) {
    reset();
    const auto historicalPath = modern ? modernStorePathForBook(old, "epub") : clipping.storeFilePath;
    HalFile f = Storage.open(historicalPath.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    assert(f && serialization::tryWritePod(f, uint8_t{4}) && serialization::tryWritePod(f, uint16_t{1}));
    assert(serialization::tryWriteString(f, "Title") && serialization::tryWriteString(f, "Author") &&
           serialization::tryWriteString(f, old));
    for (uint16_t value :
         {uint16_t{0}, uint16_t{0}, uint16_t{0}, uint16_t{1}, uint16_t{0}, uint16_t{0}, uint16_t{1}, uint16_t{2}})
      assert(serialization::tryWritePod(f, value));
    assert(serialization::tryWritePod(f, uint32_t{123}) && serialization::tryWritePod(f, uint32_t{456}));
    if (modern) {
      assert(serialization::tryWritePod(f, uint32_t{100}) && serialization::tryWritePod(f, uint32_t{200}) &&
             serialization::tryWritePod(f, uint64_t{7}) && serialization::tryWritePod(f, uint8_t{0}));
      char id[65] = "historic-id";
      assert(f.write(id, sizeof(id)) == sizeof(id));
    } else {
      assert(serialization::tryWritePod(f, uint16_t{7}));
    }
    char chapter[48] = "Chapter";
    assert(f.write(chapter, sizeof(chapter)) == sizeof(chapter) && serialization::tryWritePod(f, uint16_t{7}));
    assert(f.write("history", 7) == 7 && f.sync() && f.close());
    const auto v4 = disk.at(historicalPath);
    std::string journal(65, '\0');
    journal.replace(0, 7, "deleted");
    journal += "tail";
    disk[historicalPath + ".deleted"] = journal;
    const auto blocked = disk;
    assert(move() == BookMoveUtils::RenameMigrationResult::RolledBack && disk == blocked);
    assert(disk.at(historicalPath) == v4 && disk.at(historicalPath + ".deleted") == journal);
    disk.erase(historicalPath + ".deleted");  // Explicit fresh fixture without a pending deletion journal.
    assert(move() == BookMoveUtils::RenameMigrationResult::Success);
    ClippingStore migrated;
    assert(migrated.loadForBook(next, "", "", "epub") && migrated.clippingCount() == 1);
    assert(migrated.readClippingText(size_t{0}, reopenedText) && reopenedText == "history");
    assert(migrated.clippingAt(0)->timestamp == 123 && migrated.clippingAt(0)->layoutSignature == 456);
    assert(migrated.clippingAt(0)->tableSelection == (modern ? UINT16_MAX : 7));
    assert(disk.at(clipDest)[0] == 5 && !disk.count(historicalPath));
    std::cout << "CASE clipping-genuine-v4/modern=" << modern << "\n";
  }
  for (bool alias : {false, true}) {
    reset();
    disk[clipping.storeFilePath] = clipSource;
    failRenameTo = next;
    mutateRenameFailure = alias;
    assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain);
    assert(disk.at(old) == "BOOK_FILE" && disk.at(src) == sourceBytes &&
           disk.at(clipping.storeFilePath) == clipSource && disk.count(clipDest));
    assert(disk.count(next) == size_t(alias) && disk.count("/.crosspoint/book-move.pending"));
    const auto uncertain = disk;
    assert(move() == BookMoveUtils::RenameMigrationResult::Uncertain && disk == uncertain);
    std::cout << "CASE composed-book-negative-ack/alias=" << alias << "\n";
  }
  reset();
  // Exercise shared-node storage identity while cursors and close state remain
  // independent. An exclusive-create collision may not truncate live bytes.
  HalFile first = Storage.open(src.c_str()), clone = first;
  char byte = 0;
  assert(first.read(&byte, 1) == 1 && first.position() == 1 && clone.position() == 0);
  assert(first.close() && !first && clone && clone.size() == sourceBytes.size());
  assert(!Storage.open(src.c_str(), O_WRONLY | O_CREAT | O_EXCL) && disk.at(src) == sourceBytes);
  assert(clone.seek(sourceBytes.size()) && clone.available() == 0 && !clone.seek(sourceBytes.size() + 1));
  assert(clone.close());
  std::vector<ClippedBookEntry> entries;
  disk[clipping.storeFilePath] = clipSource;
  const auto beforeScan = disk;
  assert(ClippingStore::getAllClippedBooks(entries) && entries.size() == 1 && entries[0].bookPath == old);
  assert(disk == beforeScan);
  std::cout << "PASS nested qualified HAL independent cursors exclusive-create size seek and root iteration\n";
}
