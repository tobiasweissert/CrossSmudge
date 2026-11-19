bool has(const GfxRenderer& r, const std::string& s) {
  return std::find(r.labels.begin(), r.labels.end(), s) != r.labels.end();
}
int main() {
  using P = ReaderStatusBarPosition;
  using I = ReaderStatusBarItem;
  for (bool landscape : {false, true}) {
    GfxRenderer r;
    if (landscape) {
      r.width = 800;
      r.screenHeight = 480;
    }
    SETTINGS.topReaderStatusBar = {};
    SETTINGS.bottomReaderStatusBar = {};
    assert(ReaderChrome::height(r, P::Top) == 0);
    assert(!ReaderChrome::tapped(r, 3));
    SETTINGS.topReaderStatusBar.slots[0] = I::Clock;
    SETTINGS.topReaderStatusBar.slots[1] = I::Date;
    halClock.available = false;
    assert(ReaderChrome::height(r, P::Top) == 0);
    assert(SETTINGS.topReaderStatusBar.slots[0] == I::Clock);
    halClock.available = true;
    assert(ReaderChrome::height(r, P::Top) > 0);
    SETTINGS.topReaderStatusBar.slots = {I::Clock,
                                         I::Battery,
                                         I::TimeLeftBook,
                                         I::TitleBook,
                                         I::ChapterPageCount,
                                         I::StablePageNumber,
                                         I::BookProgressPercentage};
    SETTINGS.topReaderStatusBar.percentageFormat = 2;
    SETTINGS.topReaderStatusBar.progressBar = CrossPointSettings::CHAPTER_PROGRESS;
    SETTINGS.topReaderStatusBar.progressBarThickness = 2;
    SETTINGS.bottomReaderStatusBar.slots = {I::Date,  I::TimeLeftChapter, I::TitleChapter, I::Empty,
                                            I::Empty, I::Empty,           I::Empty};
    ReaderChrome::Values v;
    v.bookTitle = "Book";
    v.chapterTitle = "Chapter";
    v.timeBook = "90s";
    v.timeChapter = "20s";
    v.page = 3;
    v.pages = 10;
    v.stablePage = 23;
    v.stablePages = 100;
    v.bookPercent = 22.25f;
    v.chapterPercent = 30;
    v.bookmarked = true;
    auto top = ReaderChrome::band(r, P::Top), bottom = ReaderChrome::band(r, P::Bottom);
    assert(top.y == 3 && top.height == 28 && bottom.y + bottom.height == r.screenHeight - 7);
    assert(ReaderChrome::tapped(r, top.y) && ReaderChrome::tapped(r, top.y + top.height - 1));
    assert(!ReaderChrome::tapped(r, top.y + top.height));
    assert(ReaderChrome::tapped(r, bottom.y) && !ReaderChrome::tapped(r, bottom.y - 1));
    ReaderChrome::draw(r, P::Top, v);
    assert(has(r, "Book") && has(r, "90s") && has(r, "3/10") && has(r, "23/100") && has(r, "22.25%"));
    r.labels.clear();
    ReaderChrome::draw(r, P::Bottom, v);
    assert(has(r, "Jan 01") && has(r, "20s") && has(r, "Chapter") && !has(r, "Book"));
    r.labels.clear();
    ReaderChrome::draw(r, P::Bottom, v, false);
    assert(r.labels.empty());
    for (int raw = 0; raw < static_cast<int>(I::Count); ++raw) {
      SETTINGS.topReaderStatusBar.slots.fill(static_cast<I>(raw));
      r.labels.clear();
      ReaderChrome::draw(r, P::Top, v);
      assert(r.labels.size() == 7);
    }
    SETTINGS.xtcStatusBarMode = CrossPointSettings::XTC_STATUS_BAR_TOP;
    SETTINGS.legacyXtcTopUsesBottom = true;
    assert(&ReaderChrome::config(P::Top, true) == &SETTINGS.bottomReaderStatusBar);
    assert(ReaderChrome::height(r, P::Bottom, true) == 0);
    SETTINGS.legacyXtcTopUsesBottom = false;
    assert(&ReaderChrome::config(P::Top, true) == &SETTINGS.topReaderStatusBar);
    SETTINGS.xtcStatusBarMode = CrossPointSettings::XTC_STATUS_BAR_HIDE;
    assert(!ReaderChrome::tapped(r, 3, true));
    SETTINGS.xtcStatusBarMode = CrossPointSettings::XTC_STATUS_BAR_BOTTOM;
    XtcReaderActivity x;
    x.renderer = r;
    SETTINGS.bottomReaderStatusBar.slots = {
        I::TitleBook,        I::TitleChapter,     I::TimeLeftBook,          I::TimeLeftChapter,
        I::ChapterPageCount, I::StablePageNumber, I::BookProgressPercentage};
    SETTINGS.bottomReaderStatusBar.percentageFormat = 0;
    x.renderer.labels.clear();
    x.renderStatusBarOverlay(XtcReaderActivity::StatusBarOverlayPosition::Bottom, 22);
    assert(has(x.renderer, "Book") && has(x.renderer, "Chapter") && has(x.renderer, "770s") && has(x.renderer, "70s") &&
           has(x.renderer, "3/10") && has(x.renderer, "23/100"));
    x.xtc->chapters = false;
    x.renderer.labels.clear();
    x.renderStatusBarOverlay(XtcReaderActivity::StatusBarOverlayPosition::Bottom, 22);
    assert(has(x.renderer, "23/100") && !has(x.renderer, "Chapter") && !has(x.renderer, "3/10") &&
           !has(x.renderer, "70s"));
    x.statusBarVisible = false;
    x.renderer.labels.clear();
    x.renderer.fills.clear();
    x.renderStatusBarOverlay(XtcReaderActivity::StatusBarOverlayPosition::Bottom, 22);
    assert(x.renderer.labels.empty() && x.renderer.fills.size() == 1);
  }
  std::cout << "chrome: real 7-slot draw/geometry/XTC consumers, portrait+landscape PASS\n";
}
