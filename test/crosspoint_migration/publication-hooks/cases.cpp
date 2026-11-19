int cases = 0;
void reset() {
  disk.clear();
  clearCalls = 0;
  dirty = mutations = renameCalls = failRename = 0;
  mutateFailure = false;
  removeOK = closeOK = writeOK = readOK = syncOK = true;
  BookMoveUtils::calls = BookActions::calls = 0;
  BookActions::result = true;
  BookMoveUtils::result = BookMoveUtils::RenameMigrationResult::Success;
  disk["/src.epub"] = {false, "book"};
}
int main() {
  for (bool ok : {false, true})
    for (bool uncertain : {false, true})
      for (const char* path : {"/received.epub", "/received.txt", "/received.inkapp", ""}) {
        reset();
        airprobe::finishOK = ok;
        airprobe::uncertain = uncertain;
        airprobe::Context c;
        c.storage.path = path;
        airprobe::status = {};
        assert(airprobe::finishFile(&c) == ok);
        assert(dirty == (ok && library::isLibraryBookFile(path)));
        assert(c.fileCommitted == ok);
        assert(airprobe::lockDepth == 0);
        assert(airprobe::status.state == (ok ? airprobe::State::Success : airprobe::State::Error));
        ++cases;
      }
  for (auto result :
       {BookMoveUtils::RenameMigrationResult::Success, BookMoveUtils::RenameMigrationResult::RolledBack,
        BookMoveUtils::RenameMigrationResult::KeepRenamed, BookMoveUtils::RenameMigrationResult::Uncertain}) {
    reset();
    BookMoveUtils::result = result;
    WebServer s;
    WebDAVHandler h;
    h.handleMove(s);
    assert(BookMoveUtils::calls == 1 && clearCalls == 0);
    assert(s.code == (result == BookMoveUtils::RenameMigrationResult::Success ? 201 : 409));
    assert(dirty == (result == BookMoveUtils::RenameMigrationResult::Success));
    assert(mutations == 0);
    ++cases;
  }
  for (bool overwrite : {false, true}) {
    reset();
    disk["/dst.epub"] = {false, "original"};
    WebServer s;
    s.overwrite = overwrite;
    WebDAVHandler h;
    h.handleMove(s);
    assert(s.code == (overwrite ? 409 : 412));
    assert(BookMoveUtils::calls == 0 && mutations == 0 && dirty == 0);
    assert(disk.at("/dst.epub").bytes == "original");
    ++cases;
  }
  for (bool ok : {false, true}) {
    reset();
    removeOK = ok;
    disk["/collection"] = {true, {}};
    WebServer s;
    s.path = "/collection";
    WebDAVHandler h;
    h.handleDelete(s);
    assert(s.code == (ok ? 204 : 500));
    assert(dirty == ok);
    ++cases;
  }
  for (bool ok : {false, true}) {
    reset();
    BookActions::result = ok;
    WebServer s;
    WebDAVHandler h;
    h.handleDelete(s);
    assert(s.code == (ok ? 204 : 500));
    assert(BookActions::calls == 1 && mutations == 0);
    ++cases;
  }
  reset();
  disk["/collection"] = {true, {}};
  {
    WebServer s;
    s.path = "/collection";
    WebDAVHandler h;
    h.handleMove(s);
    assert(s.code == 409 && mutations == 0 && dirty == 0);
    ++cases;
  }
  for (const char* marker : {"/.crosspoint/book-move.pending", "/.crosspoint/clippings/move.pending"}) {
    reset();
    disk[marker] = {false, "intent"};
    disk["/collection"] = {true, {}};
    WebServer s;
    s.path = "/collection";
    WebDAVHandler h;
    h.handleDelete(s);
    assert(s.code == 409 && mutations == 0 && dirty == 0);
    assert(!cacheprobe::clearBookCachePreservingUserState("/src.epub"));
    assert(mutations == 0);
    ++cases;
  }
  for (bool ok : {false, true}) {
    reset();
    WebServer s;
    WebDAVHandler h;
    h._putOk = ok;
    h.handlePut(s);
    assert(s.code == (ok ? 201 : 500));
    assert(dirty == ok && mutations == 0);
    ++cases;
  }
  for (bool fail : {false, true})
    for (bool after : {false, true}) {
      reset();
      failRename = fail ? 1 : 0;
      mutateFailure = after;
      WebServer s;
      WebDAVHandler h;
      h.handleCopy(s);
      assert(s.code == (fail ? 409 : 201));
      assert(dirty == !fail);
      assert(disk.count(fail && !after ? "/dst.epub.davcopytmp" : "/dst.epub"));
      ++cases;
    }
  for (bool close : {false, true}) {
    reset();
    closeOK = close;
    WebServer s;
    WebDAVHandler h;
    h.handleCopy(s);
    assert(s.code == (close ? 201 : 409));
    assert(dirty == close);
    if (!close) assert(disk.count("/dst.epub.davcopytmp"));
    ++cases;
  }
  for (int mode = 0; mode < 5; ++mode) {
    reset();
    WebServer s;
    WebDAVHandler h;
    HTTPRaw raw;
    if (mode == 0)
      disk[s.path] = {false, "original"};
    else
      disk.erase(s.path);
    if (mode == 1) disk[s.path + ".davtmp"] = {false, "retained"};
    if (mode == 2) disk["/.crosspoint/book-move.pending"] = {false, "intent"};
    h.raw(s, "", raw);
    if (mode < 3) {
      assert(!h._putOk && mutations == 0);
    } else {
      assert(h._putOk);
      if (mode == 4) {
        failRename = 1;
        mutateFailure = true;
      }
      raw.status = RAW_END;
      h.raw(s, "", raw);
      h.handlePut(s);
      assert(s.code == (mode == 4 ? 500 : 201));
      assert(dirty == (mode == 3));
      assert(disk.count(s.path));
    }
    ++cases;
  }
  for (const char* path : {"/a.epub", "/a.txt", "/a.xtc"})
    for (bool derived : {false, true}) {
      reset();
      std::string cache = FsHelpers::hasEpubExtension(path)  ? "/cache/epub"
                          : FsHelpers::hasTxtExtension(path) ? "/cache/txt"
                                                             : "/cache/xtc";
      disk[cache] = {true, {}};
      disk[cache + "/reading_stats_off"] = {false, "OFF"};
      disk[cache + "/reading_stats_off.tmp"] = {false, "PENDING"};
      disk[cache + "/derived.bin"] = {false, "derived"};
      assert(derived ? cacheprobe::clearBookCacheDirectoryPreservingStats(cache)
                     : cacheprobe::clearBookCachePreservingUserState(path));
      assert(disk.at(cache + "/reading_stats_off").bytes == "OFF");
      assert(disk.at(cache + "/reading_stats_off.tmp").bytes == "PENDING");
      assert(!disk.count(cache + "/derived.bin"));
      ++cases;
    }
  for (bool after : {false, true})
    for (int fail : {1, 2, 3, 4}) {
      reset();
      std::string c = "/cache/epub";
      disk[c] = {true, {}};
      disk[c + "/reading_stats_off"] = {false, "OFF"};
      disk[c + "/reading_stats_off.tmp"] = {false, "PENDING"};
      failRename = fail;
      mutateFailure = after;
      assert(!cacheprobe::clearBookCachePreservingUserState("/a.epub"));
      assert(renameCalls == fail);
      auto snapshot = disk;
      int writes = mutations;
      assert(!cacheprobe::clearBookCachePreservingUserState("/a.epub"));
      assert(mutations == writes);
      assert(disk.size() == snapshot.size());
      ++cases;
    }
  reset();
  disk["/cache/epub"] = {true, {}};
  disk["/cache/epub.upload_preserve_reading_stats_off.tmp"] = {false, "retained"};
  assert(!cacheprobe::clearBookCachePreservingUserState("/a.epub"));
  assert(mutations == 0);
  ++cases;
  reset();
  disk["/cache/epub"] = {true, {}};
  disk["/cache/epub/reading_stats_off.tmp"] = {false, "pending"};
  removeOK = false;
  assert(!cacheprobe::clearBookCachePreservingUserState("/a.epub"));
  assert(renameCalls == 1);
  assert(disk.count("/cache/epub.upload_preserve_reading_stats_off.tmp"));
  ++cases;
  for (int fault = 0; fault < 4; ++fault) {
    reset();
    if (fault == 0) readOK = false;
    if (fault == 1) writeOK = false;
    if (fault == 2) syncOK = false;
    if (fault == 3) closeOK = false;
    WebServer s;
    WebDAVHandler h;
    h.handleCopy(s);
    assert(s.code == 409 && dirty == 0 && renameCalls == 0);
    assert(disk.count("/dst.epub.davcopytmp"));
    ++cases;
  }
  for (int fault = 0; fault < 4; ++fault) {
    reset();
    disk["/cache/epub"] = {true, {}};
    disk["/cache/epub/reading_stats_off.tmp"] = {false, "pending"};
    if (fault == 0) readOK = false;
    if (fault == 1) writeOK = false;
    if (fault == 2) syncOK = false;
    if (fault == 3) closeOK = false;
    assert(!cacheprobe::clearBookCachePreservingUserState("/a.epub"));
    assert(renameCalls == 0);
    assert(disk.at("/cache/epub/reading_stats_off.tmp").bytes == "pending");
    assert(disk.count("/cache/epub.cache-clear.pending"));
    readOK = writeOK = syncOK = closeOK = true;
    int writes = mutations;
    assert(!cacheprobe::clearBookCachePreservingUserState("/a.epub"));
    assert(writes == mutations);
    ++cases;
  }
  for (bool dir : {false, true}) {
    reset();
    closeOK = false;
    disk["/src.epub"].dir = dir;
    WebServer s;
    WebDAVHandler h;
    h.handleDelete(s);
    assert(s.code == 409 && BookActions::calls == 0 && mutations == 0 && dirty == 0);
    ++cases;
  }
  reset();
  assert(cacheprobe::clearBookCachePreservingUserState("/missing.epub"));
  assert(mutations == 0);
  ++cases;
  std::cout << "PASS " << cases << " production-body publication/cache hardware-double cases; no physical FAT claim\n";
}
