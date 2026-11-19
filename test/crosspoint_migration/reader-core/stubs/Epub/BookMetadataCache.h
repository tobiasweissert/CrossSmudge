#pragma once
#include <string>
class BookMetadataCache {
 public:
  struct BookMetadata {
    std::string title, language, coverItemHref;
  };
  explicit BookMetadataCache(const std::string&) {}
  bool beginWrite() { return true; }
  bool beginContentOpfPass() { return true; }
  bool endContentOpfPass() { return true; }
  bool beginTocPass() { return true; }
  bool endTocPass() { return true; }
  bool endWrite() { return true; }
  bool load() { return true; }
  void createSpineEntry(const char*) {}
  void createTocEntry(const std::string&, const char*, const char*, int) {}
  bool buildBookBin(const std::string&, const BookMetadata&) { return true; }
  void cleanupTmpFiles() {}
};
