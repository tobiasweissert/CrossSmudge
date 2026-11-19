int main() {
  EpubReaderBookmarksActivity list;
  list.openSelectedBookmark();
  assert(!list.finished);
  BookmarkEntry b{};
  b.xpath = "/body/DocFragment[8]/body/p[11]";
  b.percentage = .375f;
  b.computedSpineIndex = 7;
  b.computedChapterPageCount = 24;
  b.computedChapterProgress = 9;
  list.bookmarks.push_back(b);
  list.openSelectedBookmark();
  assert(list.finished && list.result.xpath == b.xpath && list.result.percentage == .375f);
  assert(list.result.hasSavedProgress && list.result.spineIndex == 7 && list.result.page == 9 &&
         list.result.totalPages == 24);
  assert(!list.result.hasVisibleTextOffset);
  list.bookmarks[0].hasVisibleTextOffset = true;
  list.bookmarks[0].visibleTextOffset = 12345;
  list.bookmarks[0].computedChapterProgress = 99;
  list.openSelectedBookmark();
  assert(list.result.hasVisibleTextOffset && list.result.visibleTextOffset == 12345 && list.result.spineIndex == 7);
  assert(list.result.page == 0 && list.result.totalPages == 0 && list.result.xpath == b.xpath);
  list.bookmarks[0].computedSpineIndex = 99;
  list.openSelectedBookmark();
  assert(list.result.spineIndex == 99 && list.result.visibleTextOffset == 12345 && list.result.totalPages == 0);
  std::cout
      << "PASS retained legacy open body: XPath/progress, exact spine-relative offset and invalid page-hint fallback\n";
}
