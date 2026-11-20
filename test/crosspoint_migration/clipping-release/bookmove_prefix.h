#include <ClippingStore.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "util/BookMoveUtils.h"
#define LOG_ERR(...) ((void)0)
struct Epub {
  static std::string cachePathForFilePath(const std::string& s, const char*) { return "/.crosspoint/cache" + s; }
};
struct Txt {
  std::string p;
  Txt(const std::string& s, const char*) : p(s) {}
  std::string getCachePath() { return "/.crosspoint/txt" + p; }
};
struct Xtc {
  std::string p;
  Xtc(const std::string& s, const char*) : p(s) {}
  std::string getCachePath() { return "/.crosspoint/xtc" + p; }
};
namespace FileActionPaths {
inline bool isUserPath(const std::string& s) { return s.starts_with("/"); }
}  // namespace FileActionPaths
namespace library {
inline void markLibraryIndexDirty() {}
}  // namespace library
struct BookmarkStore {
  inline static unsigned begins = 0;
  struct RenameMigration {};
  static bool beginRenameMigration(const std::string&, const std::string&, const std::string&, const std::string&,
                                   const char*, RenameMigration&, bool) {
    ++begins;
    return true;
  }
  static bool commitRenameMigration(RenameMigration&) { return true; }
  static bool rollbackRenameMigration(RenameMigration&) { return true; }
};
struct StateFixture {
  std::string openEpubPath;
  bool saveToFile() { return true; }
} APP_STATE;
struct RecentFixture {
  bool updatePath(const std::string&, const std::string&, const std::string&, const std::string&) { return true; }
} RECENT_BOOKS;
