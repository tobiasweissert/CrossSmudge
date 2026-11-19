#include <HalStorage.h>
#include <uzlib.h>

#include <cassert>
#include <iostream>

#include "BookmarkStore.h"
#include "components/themes/BaseTheme.h"

// Independent legacy wire builder: do not serialize the native struct/padding.
template <typename T>
void pod(std::string& out, T value) {
  out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}
void str(std::string& out, const std::string& text) {
  pod(out, static_cast<uint32_t>(text.size()));
  out += text;
}
std::string fixture(uint8_t version, const std::string& path) {
  std::string out;
  pod(out, version);
  if (version == 2)
    pod(out, uint8_t{1});
  else
    pod(out, uint16_t{1});
  str(out, "Title");
  str(out, "Author");
  str(out, path);
  pod(out, uint16_t{7});
  pod(out, .375f);
  pod(out, uint32_t{99});
  std::string title = "Chapter";
  title.resize(48, '\0');
  out += title;
  if (version >= 4) pod(out, uint16_t{11});
  if (version >= 5) {
    std::string snippet = "Original snippet";
    snippet.resize(64, '\0');
    out += snippet;
  }
  return out;
}
int main() {
  const std::string book = "/Books/A.epub";
  const auto crc = uzlib_crc32(book.data(), book.size(), 0);
  const auto path = "/.crosspoint/bookmarks/epub_" + std::to_string(crc) + ".bin";
  for (uint8_t version = 2; version <= 5; ++version) {
    BOOKMARKS.unload();
    bookmarkDisk.clear();
    const std::string bytes = fixture(version, book);
    bookmarkDisk[path] = bytes;
    assert(BOOKMARKS.loadForBook(book, "Title", "Author", "epub"));
    const auto& entries = BOOKMARKS.getBookmarks();
    assert(entries.size() == 1);
    const auto& b = entries[0];
    assert(b.spineIndex == 7 && b.progress == .375f && b.timestamp == 99);
    assert(std::string(b.chapterTitle) == "Chapter");
    assert(b.paragraphIndex == (version >= 4 ? 11 : UINT16_MAX));
    assert(std::string(b.snippet) == (version >= 5 ? "Original snippet" : ""));
    if (version == 5) assert(bookmarkDisk[path] == bytes);
    assert(BOOKMARKS.hasBookmarkForPage(7, .375f, 8));
    assert(!BOOKMARKS.hasBookmarkForPage(8, .375f, 8));
    assert(!BOOKMARKS.removeBookmarkAt(1) && BOOKMARKS.getBookmarks().size() == 1);
  }
  BOOKMARKS.unload();
  bookmarkDisk.clear();
  assert(BOOKMARKS.loadForBook(book, "Title", "Author", "epub"));
  assert(BOOKMARKS.addBookmark(7, .375f, 8, "Chapter", 11, "Original snippet") == BookmarkStore::AddResult::Added);
  auto expected = fixture(5, book);
  // addBookmark reserves timestamp zero, unlike the retained timestamp fixture.
  const size_t timestamp = 1 + 2 + 4 + 5 + 4 + 6 + 4 + book.size() + 2 + 4;
  std::memset(expected.data() + timestamp, 0, 4);
  assert(bookmarkDisk.at(path) == expected);
  assert(BOOKMARKS.addBookmark(7, .375f, 8, "Chapter", 11, "Original snippet") == BookmarkStore::AddResult::Added);
  assert(BOOKMARKS.getBookmarks().size() == 1);
  assert(BOOKMARKS.removeBookmarkAt(0) && BOOKMARKS.getBookmarks().empty());
  std::cout << "PASS full BookmarkStore TU: v2-v5 progress/anchors, exact v5 wire bytes, page identity, replacement "
               "and deletion\n";
}
