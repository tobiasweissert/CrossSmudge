// Narrow dependencies for unchanged production EPUB viewport and chrome consumers.
constexpr int STR_AUTO_TURN_ENABLED = 3;
namespace ReaderUtils {
constexpr int TOP_CLOCK_TEXT_PADDING = 0;
}
struct Section {
  int currentPage = 2, pageCount = 10;
  int estimatedTotalPages() const { return 10; }
  bool isBuilding() const { return false; }
  bool isPartial() const { return false; }
};
struct Epub {
  std::string getTitle() const { return "EpubBook"; }
  bool resolveReferencePage(int, float, uint32_t& p, uint32_t& n) const {
    p = 23;
    n = 100;
    return true;
  }
  void resolveChapterGroupRange(int, int& a, int& b) const {
    a = 0;
    b = 1;
  }
  int getTocIndexForSpineIndex(int) const { return 0; }
  struct Toc {
    std::string title = "EpubChapter";
  };
  Toc getTocItem(int) const { return {}; }
};
struct Bookmarks {
  bool hasBookmarkForPage(uint16_t, float, int) const { return true; }
} BOOKMARKS;
struct EpubReaderActivity {
  struct Touch {
    bool tapped;
    int x, y;
  };
  int toggles = 0, updates = 0;
  void handleTap(Touch touch);
  void executeReaderQuickAction(int) { ++toggles; }
  void requestUpdate() { ++updates; }
  GfxRenderer renderer;
  std::unique_ptr<Section> section = std::make_unique<Section>();
  std::unique_ptr<Epub> epub = std::make_unique<Epub>();
  bool statusBarVisible = true, activeFootnotePreview = false, automaticPageTurnActive = false;
  unsigned long pageTurnDuration = 30000;
  int currentSpineIndex = 0;
  float getCurrentBookProgressPercent() const { return 22.25f; }
  void resolveChapterGroupPageProgress(int& p, int& n, float& progress, bool& estimated) const {
    p = 3;
    n = 10;
    progress = .3f;
    estimated = false;
  }
  bool estimateTimeLeftSeconds(bool book, uint32_t& seconds) const {
    seconds = book ? 900 : 70;
    return true;
  }
  bool getTimeLeftPaceSeconds(uint16_t&, const char*&, uint16_t&) const { return false; }
  bool estimateRemainingTimeLeftPages(bool, float&) const { return true; }
  bool formatTimeLeftLabel(char*, size_t, uint8_t = 255) const;
  void renderStatusBar() const;
};
struct Txt {
  std::string getTitle() const { return "TextBook"; }
};
struct TxtReaderActivity {
  GfxRenderer renderer;
  bool statusBarVisible = true;
  int totalPages = 50, currentPage = 10;
  std::unique_ptr<Txt> txt = std::make_unique<Txt>();
  void renderStatusBar() const;
};
