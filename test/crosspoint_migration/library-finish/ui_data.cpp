// Real activity/index source; rendering, recents/settings/BookActions are explicit doubles.
#include <cassert>
#include <iostream>

#include "boundaries.h"
#include "ui_classes.h"
int main() {
  fake::reset();
  fake::add("/a.txt");
  fake::add("/b.md");
  fake::add("/c.epub");
  bookMetadata["/c.epub"].series = "Saga";
  bookMetadata["/c.epub"].genre = "History";
  library::BuildStats stats;
  assert(library::buildLibraryIndex("/", stats, true));
  GfxRenderer renderer;
  MappedInputManager input;
  SETTINGS.librarySortMethod = 4;
  LibraryListActivity screen(renderer, input);
  screen.onEnter();
  assert(screen.simulatorRowCount() == 3);
  RecentBook book;
  screen.simulatorSetView(1, false, "Tit");
  assert(screen.simulatorRowCount() == 1);
  assert(screen.simulatorReadBook(0, book));
  assert(book.path == "/c.epub");
  screen.simulatorSetView(1, false, "Saga");
  assert(screen.simulatorRowCount() == 1);
  screen.simulatorSetView(1, false, "History");
  assert(screen.simulatorRowCount() == 1);
  screen.simulatorSetView(1, false, "");
  assert(screen.simulatorRowCount() == 3);
  SETTINGS.libraryShowMarkdown = 0;
  screen.simulatorSetView(1, false, "");
  assert(screen.simulatorRowCount() == 2);
  SETTINGS.libraryShowMarkdown = 1;
  BookActions::completedPaths.push_back("/c.epub");
  SETTINGS.libraryHideFinishedBooks = 1;
  screen.simulatorSetView(1, false, "");
  assert(screen.simulatorRowCount() == 2);
  SETTINGS.libraryHideFinishedBooks = 0;
  RECENT_BOOKS.books = {{"/b.md", "b", "", ""}, {"/a.txt", "a", "", ""}};
  screen.simulatorSetView(4, true, "");
  assert(screen.simulatorRowCount() == 2);
  assert(screen.simulatorReadBook(0, book));
  assert(book.path == "/b.md");
  screen.simulatorSetView(4, false, "");
  assert(screen.simulatorReadBook(0, book));
  assert(book.path == "/a.txt");
  screen.simulatorSetView(0, true, "");
  assert(screen.simulatorRowCount() == 3);
  assert(screen.simulatorReadBook(0, book));
  assert(book.path == "/b.md");
  SETTINGS.libraryRecentOpenedOnly = 1;
  screen.simulatorSetView(0, true, "");
  assert(screen.simulatorRowCount() == 2);
  SETTINGS.libraryRecentOpenedOnly = 0;
  fake::add("/another.txt");
  library::markLibraryIndexDirty();
  screen.simulatorSetView(1, false, "");
  assert(screen.simulatorRowCount() == 4);
  assert(screen.simulatorCollapseGroup(0));
  assert(screen.simulatorRowCount() == 3);
  screen.simulatorExpandGroup(0);
  assert(screen.simulatorRowCount() == 4);
  RECENT_BOOKS.books.clear();
  for (int i = 0; i < 18; ++i) {
    auto path = "/recent" + std::to_string(i) + ".txt";
    fake::add(path);
    RECENT_BOOKS.books.push_back({path, path, "", ""});
  }
  library::markLibraryIndexDirty();
  screen.simulatorSetView(0, true, "");
  assert(screen.simulatorRowCount() == 22);
  for (int i = 0; i < 18; ++i) {
    assert(screen.simulatorReadBook(i, book));
    assert(book.path == RECENT_BOOKS.books[i].path);
  }
  screen.simulatorSetView(1, false, "Saga");
  assert(screen.simulatorRowCount() == 1);
  assert(!screen.simulatorCollapseGroup(0));
  LibrarySettingsActivity settings(renderer, input);
  SETTINGS.saveSucceeds = false;
  const auto saved = SETTINGS.libraryShowEpub;
  settings.simulatorToggle(6);
  assert(SETTINGS.libraryShowEpub == saved);
  SETTINGS.saveSucceeds = true;
  settings.simulatorToggle(10);
  assert(SETTINGS.libraryRecentOpenedOnly == 1);
  settings.simulatorToggle(10);
  assert(SETTINGS.libraryRecentOpenedOnly == 0);
  screen.simulatorSetView(1, false, "");
  SETTINGS.libraryListExpanded = 0;
  screen.render(RenderLock{});
  confirmationSubject.clear();
  screen.simulatorSwipeDelete(240, 5);
  assert(confirmationSubject.empty());
  bool reachedSecond = false;
  for (int y = 80; y < 650; y += 10) {
    screen.render(RenderLock{});
    confirmationSubject.clear();
    screen.simulatorSwipeDelete(240, y);
    if (confirmationSubject == "b") reachedSecond = true;
  }
  assert(reachedSecond);
  BookActions::clearedMetadata.clear();
  screen.simulatorPromptDelete(0);
  assert(lastResultHandler);
  fake::failRemovePath = "/a.txt";
  lastResultHandler(ActivityResult{});
  assert(Storage.exists("/a.txt"));
  assert(BookActions::clearedMetadata.empty());
  fake::failRemovePath.clear();
  lastResultHandler(ActivityResult{});
  assert(!Storage.exists("/a.txt"));
  assert(BookActions::clearedMetadata == std::vector<std::string>{"/a.txt"});
  screen.onExit();
  std::cout << "UI data adapter: metadata search, filters, recents, groups, save rollback, painted swipe target and "
               "failed-delete state retention PASS\n";
}
