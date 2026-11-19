#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "BookmarkEntry.h"
#include "activities/ActivityResult.h"
struct Epub {
  int getSpineItemsCount() const { return 20; }
};
struct EpubReaderBookmarksActivity {
  std::shared_ptr<Epub> epub = std::make_shared<Epub>();
  std::vector<BookmarkEntry> bookmarks;
  struct {
    int selected = 0;
  } nav;
  ProgressChangeResult result{};
  bool finished = false;
  void setResult(ProgressChangeResult value) { result = std::move(value); }
  void finish() { finished = true; }
  void openSelectedBookmark();
};
