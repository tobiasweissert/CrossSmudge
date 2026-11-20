#include <ClippingStore.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "util/BookMoveUtils.h"
#define LOG_ERR(...) ((void)0)
namespace caller {
inline BookMoveUtils::RenameMigrationResult outcome;
inline bool conservative = false;
inline int migrations = 0, refreshes = 0, invalidations = 0, metadataClears = 0, errors = 0, saves = 0;
void reset() {
  conservative = false;
  migrations = refreshes = invalidations = metadataClears = errors = saves = 0;
}
}  // namespace caller
namespace BookMoveUtils {
RenameMigrationResult migrateRenamedBookState(const std::string&, const std::string&, const std::string&,
                                              const std::string&, const std::string&, const char*, bool preserve) {
  ++caller::migrations;
  caller::conservative = preserve;
  return caller::outcome;
}
}  // namespace BookMoveUtils
struct RecentBook {
  std::string path, title, author;
};
struct RecentFixture {
  std::vector<RecentBook> books;
  const auto& getBooks() const { return books; }
  void removeByPath(const std::string&) {}
} RECENT_BOOKS;
struct StateFixture {
  std::string openEpubPath, favoriteSleepImagePath, favoriteBootImagePath;
  bool saveToFile() {
    ++caller::saves;
    return true;
  }
} APP_STATE;
namespace FsHelpers {
inline bool isSafePathComponent(const std::string& s) { return !s.empty() && s.find('/') == std::string::npos; }
inline std::string extractFolderPath(const std::string& s) { return s.substr(0, s.rfind('/') + 1); }
inline bool hasEpubExtension(const std::string& s) { return s.ends_with(".epub"); }
inline bool hasTxtExtension(const std::string& s) { return s.ends_with(".txt"); }
inline bool hasMarkdownExtension(const std::string& s) { return s.ends_with(".md"); }
}  // namespace FsHelpers
namespace FileActionPaths {
inline bool mutationBlocked = false;
inline bool isUserPath(const std::string& s) { return !s.empty() && s[0] == '/'; }
}  // namespace FileActionPaths
namespace BookActions {
inline bool clearFileMetadata(const std::string& s) {
  ++caller::metadataClears;
  return ClippingStore::deleteForFilePath(s, "epub");
}
}  // namespace BookActions
namespace ImageFolderIndex {
inline void invalidateForPath(const char*) { ++caller::invalidations; }
}  // namespace ImageFolderIndex
struct FontFixture {
  void markRegistryDirtyForPath(const char*) {}
} sdFontSystem;
inline const char* tr(int) { return "error"; }
inline constexpr int STR_FILES_MOVE_UNCERTAIN = 1, STR_FILES_UNSAFE = 2, STR_FILES_MISSING = 3,
                     STR_FILES_DELETE_FAILED = 4, STR_FILES_CACHE_CLEANUP_FAILED = 5,
                     STR_FILES_STATE_CLEANUP_FAILED = 6;
inline std::string utf8ComposeNfc(const std::string& s) { return s; }
inline std::string getFileName(const std::string& s) { return s; }
inline bool getMutationBookCache(const std::string&, std::string&, const char*& type) {
  type = "epub";
  return true;
}
inline int followListSelection(int a, int, int, int) { return a; }
struct ActivityResult {
  bool isCancelled = false;
};
struct FileBrowserActivity {
  static constexpr size_t NAME_BUFFER_SIZE = 256;
  size_t selectorIndex = 0;
  int topIndex = 0, visibleRows = 5;
  bool uiReady = true, mutationListFence = false;
  struct Gate {
    void beginPaint() {}
  } mutationListInputGate;
  struct Nav {
    int top = 0, visibleRows = 5;
    void reset(int) {}
  } listNav;
  void fileOperationError(const char*) { ++caller::errors; }
  bool isPinnedSleepFavorite(const std::string& s) { return APP_STATE.favoriteSleepImagePath == s; }
  bool isPinnedBootFavorite(const std::string& s) { return APP_STATE.favoriteBootImagePath == s; }
  void requestUpdate(bool = false) { ++caller::refreshes; }
  void loadFilesLocked() {}
  size_t findEntry(const std::string&) { return 0; }
  size_t entryCount() { return 1; }
  void renameFile(const std::string&, const std::string&, const std::string&, const std::string&);
  void deleteConfirmed(const std::string&, const ActivityResult&);
};
struct RenderLock {
  explicit RenderLock(FileBrowserActivity&) {}
};
