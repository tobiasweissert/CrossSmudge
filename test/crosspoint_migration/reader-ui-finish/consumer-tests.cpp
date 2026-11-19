bool has(const GfxRenderer& r, const std::string& s) {
  return std::find(r.labels.begin(), r.labels.end(), s) != r.labels.end();
}
int main() {
  using P = ReaderStatusBarPosition;
  using I = ReaderStatusBarItem;
  for (bool landscape : {false, true}) {
    EpubReaderActivity e;
    if (landscape) {
      e.renderer.width = 800;
      e.renderer.screenHeight = 480;
    }
    SETTINGS.topReaderStatusBar = {};
    SETTINGS.bottomReaderStatusBar = {};
    SETTINGS.topReaderStatusBar.slots = {
        I::TitleBook,        I::TitleChapter,     I::TimeLeftBook,          I::TimeLeftChapter,
        I::StablePageNumber, I::ChapterPageCount, I::BookProgressPercentage};
    auto layout = computeReaderViewportLayout(e.renderer, false);
    const auto top = ReaderChrome::band(e.renderer, P::Top), bottom = ReaderChrome::band(e.renderer, P::Bottom);
    assert(layout.marginTop >= top.y + top.height);
    assert(e.renderer.screenHeight - layout.marginBottom <= bottom.y);
    assert(layout.viewportWidth == e.renderer.width - layout.marginLeft - layout.marginRight);
    assert(layout.viewportHeight == e.renderer.screenHeight - layout.marginTop - layout.marginBottom);
    // Legacy title/time-left/stable preferences are deliberately OFF. Slots win.
    e.renderStatusBar();
    assert(has(e.renderer, "900s") && has(e.renderer, "70s") && has(e.renderer, "23/100") && has(e.renderer, "3/10"));
    SETTINGS.bottomReaderStatusBar.slots[0] = I::Battery;
    auto bookmark = ReaderChrome::band(e.renderer, P::Bottom);
    e.handleTap({true, bookmark.x, bookmark.y});
    assert(e.toggles == 1 && e.statusBarVisible);
    e.handleTap({true, bookmark.x + 20, bookmark.y});
    assert(e.toggles == 1 && !e.statusBarVisible);
    e.handleTap({true, bookmark.x, bookmark.y});
    assert(e.toggles == 1 && e.statusBarVisible);
    e.handleTap({true, bookmark.x, bookmark.y - 1});
    assert(e.toggles == 1 && e.statusBarVisible);
    e.activeFootnotePreview = true;
    e.renderer.labels.clear();
    e.renderStatusBar();
    assert(!has(e.renderer, "900s") && !has(e.renderer, "23/100"));
    e.statusBarVisible = false;
    e.renderer.labels.clear();
    e.renderStatusBar();
    assert(e.renderer.labels.empty());
    SETTINGS.topReaderStatusBar = {};
    SETTINGS.bottomReaderStatusBar = {};
    auto automatic = computeReaderViewportLayout(e.renderer, true);
    auto ordinary = computeReaderViewportLayout(e.renderer, false);
    assert(automatic.marginBottom > ordinary.marginBottom);
    assert(ReaderChrome::tapped(e.renderer, e.renderer.screenHeight - 8, false, true));
    e.activeFootnotePreview = false;
    e.statusBarVisible = true;
    e.automaticPageTurnActive = true;
    e.renderer.labels.clear();
    e.renderStatusBar();
    assert(!e.renderer.labels.empty());
    TxtReaderActivity t;
    t.renderer = e.renderer;
    t.renderer.labels.clear();
    SETTINGS.topReaderStatusBar.slots = {I::TimeLeftBook,           I::StablePageNumber, I::ChapterPageCount,
                                         I::BookProgressPercentage, I::TitleBook,        I::TitleChapter,
                                         I::TimeLeftChapter};
    t.renderStatusBar();
    assert(has(t.renderer, "22%") && has(t.renderer, "11/50") && !has(t.renderer, "900s") &&
           !has(t.renderer, "23/100"));
  }
  std::cout << "consumers: real EPUB viewport/render/TimeLeft and TXT chrome, both orientations PASS\n";
}
