#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

#include "HalStorage.h"
#include "LibraryBuilder.h"
#include "LibraryIndexFile.h"
#include "LibraryMetadataCache.h"
std::string bytes(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char** argv) {
  assert(argc == 2);
  disk::root = argv[1];
  using namespace library;
  const auto legacy = bytes(disk::root + "/.crosspoint/library.idx");
  assert(legacy.size() > 64);
  std::map<std::string, std::string> durable;
  for (auto p : {"/.crosspoint/recent.json", "/.crosspoint/cache_1/progress", "/.crosspoint/cache_1/bookmarks",
                 "/.crosspoint/cache_1/clippings", "/.crosspoint/cache_1/reading_stats",
                 "/.crosspoint/cache_1/reading_stats_off", "/apps/game/save.txt"})
    durable[p] = bytes(disk::root + p);
  BuildStats stats;
  assert(buildLibraryIndex("/", stats, false));
  assert(stats.books == 2);
  assert(bytes(disk::root + "/.crosspoint/library.idx") == legacy);
  LibraryIndexFile idx;
  assert(idx.open(libraryIndexPath()));
  assert(idx.header().formatVersion == 7);
  idx.close();
  // Only this fixture creates books after the old-format upgrade succeeded.
  for (int i = 0; i < 2000; ++i)
    std::filesystem::copy_file(disk::root + "/template.bin", disk::root + "/books/book" + std::to_string(i) + ".epub");
  const auto previous = bytes(disk::root + libraryIndexPath());
  BuildCallbacks callbacks;
  callbacks.cancelRequested = [](void*) {
    std::error_code e;
    auto n = std::filesystem::file_size(disk::root + LibraryMetadataCache::payloadPath(), e);
    return !e && n > 1024;
  };
  assert(!buildLibraryIndex("/", stats, true, &callbacks));
  assert(stats.failure == BuildFailure::Cancelled);
  assert(bytes(disk::root + libraryIndexPath()) == previous);
  assert(idx.open(libraryIndexPath()));
  assert(idx.bookCount() == 2);
  idx.close();
  assert(buildLibraryIndex("/", stats, true));
  assert(stats.metadataCached > 0);
  assert(stats.books == 2002);
  assert(idx.open(libraryIndexPath()));
  bool metadata = false;
  for (uint16_t i = 0; i < idx.bookCount(); ++i) {
    ClixRecord r{};
    std::string title, series, genre;
    assert(idx.readRecord(i, r));
    idx.readTitle(r, title);
    idx.readSeries(r, series);
    idx.readGenre(r, genre);
    if (title == "Native ZIP title") {
      assert(series == "Native Saga");
      assert(genre == "History");
      metadata = true;
    }
  }
  assert(metadata);
  idx.close();
  for (int i = 0; i < 4100; ++i) {
    std::ofstream f(disk::root + "/books/text" + std::to_string(i) + ".txt");
    f << "book";
  }
  assert(buildLibraryIndex("/", stats, true));
  assert(stats.books == 6102);
  assert(idx.open(libraryIndexPath()));
  assert(idx.bookCount() == 6102);
  idx.close();
  for (const auto& [p, b] : durable) assert(bytes(disk::root + p) == b);
  assert(bytes(disk::root + "/.crosspoint/library.idx") == legacy);
  std::cout << "Native disk: genuine V2 migration, ZIP+miniz+Expat metadata, cancelled publication/metadata resume, "
               "6102 books, durable canaries PASS\n";
}
