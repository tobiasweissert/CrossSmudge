#include <HalStorage.h>
#include <PersistableStore.h>

#include <cassert>
#include <iostream>

#include "util/BookmarkFile.h"
int main() {
  const std::string path = "/.crosspoint/bookmarks/Books_A_B.json";
  PersistableStoreBase::files[path] =
      R"({"bookmarks":[{"xpath":"/body/DocFragment[8]/body/p[11]","percentage":0.375,"summary":"Original","si":7,"pc":24,"pp":9},{"xpath":"anchor","percentage":0.625,"summary":"Second","name":"Label","si":12,"pc":18,"pp":4,"vo":0}]})";
  std::vector<BookmarkEntry> entries;
  assert(BookmarkFile::load("/Books/A\\B.epub", entries));
  assert(entries.size() == 2 && entries[0].xpath == "/body/DocFragment[8]/body/p[11]");
  assert(entries[0].percentage == .375f && entries[0].computedSpineIndex == 7);
  assert(entries[0].computedChapterPageCount == 24 && entries[0].computedChapterProgress == 9);
  assert(!entries[0].hasVisibleTextOffset && entries[1].hasVisibleTextOffset && entries[1].visibleTextOffset == 0);
  assert(BookmarkFile::save("/Books/A\\B.epub", entries));
  assert(Storage.directory == "/.crosspoint/bookmarks/");
  JsonDocument saved;
  assert(!deserializeJson(saved, PersistableStoreBase::files[path]));
  assert(saved["bookmarks"][0]["vo"].isNull() && saved["bookmarks"][0]["name"].isNull());
  assert(saved["bookmarks"][1]["vo"] == 0 && saved["bookmarks"][1]["name"] == "Label");
  assert(BookmarkFile::load("/Books/A\\B.epub", entries) && entries[1].percentage == .625f);
  entries[1].name = std::string(129, 'x');
  const auto original = PersistableStoreBase::files;
  assert(!BookmarkFile::save("/Books/A\\B.epub", entries) && original == PersistableStoreBase::files);
  PersistableStoreBase::files[path] = std::string("{\"bookmarks\":[{\"name\":\"") + std::string(129, 'a') + "\"}]}";
  assert(BookmarkFile::load("/Books/A\\B.epub", entries) && entries[0].name.size() == 128);
  assert(!BookmarkFile::load("/missing.epub", entries) && entries.empty());
  PersistableStoreBase::failWrite = true;
  assert(!BookmarkFile::save("/Books/A\\B.epub", entries));
  std::cout << "PASS legacy JSON path, progress/page hints, absent/present zero offset, labels and save refusal\n";
}
