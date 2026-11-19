struct CrossPointSettings {
  enum { BOOK_PROGRESS = 0, CHAPTER_PROGRESS = 1, HIDE_PROGRESS = 2 };
  enum { XTC_STATUS_BAR_HIDE = 0, XTC_STATUS_BAR_BOTTOM = 1, XTC_STATUS_BAR_TOP = 2 };
  enum STATUS_BAR_TITLE { BOOK_TITLE = 0, CHAPTER_TITLE = 1, HIDE_TITLE = 2 };
  enum STATUS_BAR_TIME_LEFT { TIME_LEFT_HIDE = 0, TIME_LEFT_CHAPTER = 1, TIME_LEFT_BOOK = 2 };
  ReaderStatusBarConfig topReaderStatusBar, bottomReaderStatusBar;
  bool legacyXtcTopUsesBottom = false;
  int xtcStatusBarMode = XTC_STATUS_BAR_TOP, clockUtcOffsetQ = 0, clockFormat = 0, statusBarTitle = HIDE_TITLE;
  int statusBarTimeLeft = TIME_LEFT_HIDE;
  bool publisherPageNumbers = false, stablePageNumbers = false, track = true;
  bool tapToHideStatusBar = true;
  static constexpr int LONG_MENU_TOGGLE_BOOKMARK = 44;
  uint8_t screenMarginVertical = 5, screenMarginHorizontal = 5;
  uint8_t disableReaderTouchscreen = 0;
  bool saveToFile() { return true; }
  bool shouldTrackReadingStats() const { return track; }
  struct Spec {
    int titleMode;
  };
  Spec statusBarSpec() const { return {statusBarTitle}; }
} SETTINGS;
