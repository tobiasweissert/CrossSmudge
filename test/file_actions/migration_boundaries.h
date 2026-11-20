// Explicit injected store boundaries: qualify coordinator sequencing/results,
// not real bookmark/clipping bytes, v5 codecs, or real-store atomicity.
struct BookmarkStore {
  static void deleteForFilePath(const std::string&, const char*) { events.push_back("delete-bookmarks"); }
  struct RenameMigration {
    bool prepared = false;
    bool uncertain = false;
  };
  static inline bool beginOk = true, commitOk = true, rollbackOk = true;
  static bool beginRenameMigration(const std::string& from, const std::string& to, const std::string&,
                                   const std::string&, const char*, RenameMigration& m, bool manual = false) {
    assert(!from.empty() && from != to && !m.prepared);
    if (manual) assert(Storage.exists("/.crosspoint/book-move.pending"));
    events.push_back("bookmark-prepare");
    m.prepared = beginOk;
    return beginOk;
  }
  static bool commitRenameMigration(RenameMigration& m) {
    assert(m.prepared);
    events.push_back("bookmark-commit");
    if (commitOk) m.prepared = false;
    return commitOk;
  }
  static bool rollbackRenameMigration(RenameMigration& m) {
    events.push_back("bookmark-rollback");
    if (rollbackOk) m.prepared = false;
    return rollbackOk;
  }
};
struct ClippingStore {
  using RenameMigration = BookmarkStore::RenameMigration;
  struct PreparedRename {
    RenameMigration* owner = nullptr;
    bool ready = false;
    PreparedRename() = default;
    PreparedRename(const PreparedRename&) = delete;
    PreparedRename& operator=(const PreparedRename&) = delete;
  };
  static inline bool beginOk = true, publishOk = true, commitOk = true, rollbackOk = true;
  static inline bool deleteAdmissionOk = true, deleteOk = true;
  static inline unsigned deleteAdmissions = 0;
  static bool canDeleteForFilePath(const std::string& path, const std::string& type) {
    assert(!path.empty() && type == "epub");
    ++deleteAdmissions;
    return deleteAdmissionOk;
  }
  static bool deleteForFilePath(const std::string& path, const std::string& type) {
    assert(!path.empty() && type == "epub");
    events.push_back("delete-clippings");
    return deleteAdmissionOk && deleteOk;
  }
  static bool prepareRenameMigration(const std::string& from, const std::string& to, const std::string&,
                                     const std::string&, const std::string& type, RenameMigration& m,
                                     PreparedRename& p) {
    assert(type == "epub" && !from.empty() && from != to && !m.prepared && !p.ready);
    assert(!Storage.exists("/.crosspoint/book-move.pending"));
    events.push_back("clipping-prepare");
    if (!beginOk) return false;
    p.owner = &m;
    p.ready = true;
    return true;
  }
  static bool publishPreparedRename(RenameMigration& m, PreparedRename& p) {
    assert(p.ready && p.owner == &m && !m.prepared);
    events.push_back("clipping-publish");
    p.ready = false;
    m.prepared = true;
    m.uncertain = !publishOk;
    return publishOk;
  }
  static bool commitRenameMigration(RenameMigration& m) {
    assert(!m.uncertain);
    events.push_back("clipping-commit");
    if (commitOk) m.prepared = false;
    return commitOk;
  }
  static bool rollbackRenameMigration(RenameMigration& m) {
    events.push_back("clipping-rollback");
    if (rollbackOk) m.prepared = false;
    return rollbackOk;
  }
};
