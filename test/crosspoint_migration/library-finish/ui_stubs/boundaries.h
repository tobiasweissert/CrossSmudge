#pragma once
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <FreeInkUIIcon.h>

#include "Arduino.h"
#include "Epub.h"
#include "FsHelpers.h"
#include "GfxRenderer.h"
#include "HalStorage.h"
#include "LibraryBuilder.h"
#include "LibraryFileTypes.h"
#include "LibraryIndexFile.h"
#include "LibraryInputBuffer.h"
#include "LibraryRecentOrder.h"
#include "LibraryText.h"
#include "Logging.h"
#include "Memory.h"
#include "Utf8.h"
#include "Xtc.h"
#include "ui_schema.h"
#define tr(id) uiText(StrId::id)
inline const char* uiText(StrId) { return "test"; }
struct I18n {
  static I18n& getInstance() {
    static I18n x;
    return x;
  }
  const char* get(StrId s) { return uiText(s); }
  Language getLanguage() { return Language::EnglishEn; }
};
#define I18N I18n::getInstance()
constexpr int UI_10_FONT_ID = 1, SMALL_FONT_ID = 1;
struct RenderLock {
  enum class Mode { Try };
  RenderLock() = default;
  RenderLock(Mode) {}
  static bool peek() { return false; }
  bool ownsLock() { return true; }
};
enum class RequestUpdateResult { Rendered, Rejected };
struct KeyboardResult {
  std::string text;
};
struct FileBrowserActionResult {
  int action;
};
struct OptionSelectionResult {
  uint8_t index;
};
struct ActivityResult {
  bool isCancelled = false;
  std::variant<std::monostate, KeyboardResult, FileBrowserActionResult, OptionSelectionResult> data;
};
using ActivityResultHandler = std::function<void(const ActivityResult&)>;
inline ActivityResultHandler lastResultHandler;
struct MappedInputManager {
  enum class Button { Back, Confirm, Up, Down, Left, Right };
  enum class SwipeDir { None, Up, Down, Left, Right };
  bool hasTouch() const { return true; }
  bool hasTouchHardware() const { return true; }
  bool isPressed(Button) const { return false; }
  bool wasPressed(Button) const { return false; }
  bool wasReleased(Button) const { return false; }
  unsigned long getHeldTime(Button = Button::Confirm) const { return 0; }
  bool wasScreenTouchDown(int&, int&) const { return false; }
  bool isScreenTouchHeld(int&, int&) const { return false; }
  bool wasScreenTapped(int&, int&) const { return false; }
  bool wasScreenLongPress(int&, int&) const { return false; }
  bool wasScreenTouchReleased() const { return false; }
  bool wasHomeGesture() const { return false; }
  SwipeDir wasSwipe() const { return SwipeDir::None; }
  void suppressCurrentTouchContact() {}
  void update() {}
  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) const { return {a, b, c, d}; }
  const char* withBackArrow(const char* s) { return s; }
};
struct Activity {
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  Activity(const char*, GfxRenderer& r, MappedInputManager& m) : renderer(r), mappedInput(m) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool blocksGlobalInput() const { return false; }
  virtual bool requiresExclusiveStorageLoop() const { return false; }
  virtual bool preventAutoSleep() { return false; }
  void requestUpdate(bool = false) {}
  RequestUpdateResult requestUpdateAndWait() { return RequestUpdateResult::Rendered; }
  void onGoHome() {}
  void finish() {}
  void finishAfterBackPress() {}
  void onSelectBook(const std::string&) {}
  void startActivityForResult(std::unique_ptr<Activity>&&, ActivityResultHandler h) {
    lastResultHandler = std::move(h);
  }
};
struct RecentBook {
  enum class CoverState { Unknown, Missing };
  std::string path, title, author, coverBmpPath;
  CoverState coverState = CoverState::Unknown;
};
struct RecentBooksStore {
  std::vector<RecentBook> books;
  const auto& getBooks() { return books; }
  size_t getCount() { return books.size(); }
  bool pruneMissing() { return false; }
  bool saveToFile() { return true; }
  bool updateBook(const std::string&, const std::string&, const std::string&, const std::string&,
                  RecentBook::CoverState) {
    return true;
  }
  bool removeByPath(const std::string&) { return true; }
};
inline RecentBooksStore RECENT_BOOKS;
struct CrossPointSettings {
  enum { RECENT_BOOKS_GRID, RECENT_BOOKS_LIST, DATE_SEPARATOR_PERIOD, DATE_SEPARATOR_HYPHEN };
  uint8_t libraryRecentOpenedOnly = 0, libraryUseMetadata = 1, librarySortMethod = 1, librarySortDescending = 0,
          libraryListExpanded = 1, libraryHideFinishedBooks = 0, libraryShowSeries = 1, libraryShowGenre = 1,
          libraryShowEpub = 1, libraryShowXtc = 1, libraryShowTxt = 1, libraryShowMarkdown = 1,
          recentBooksView = RECENT_BOOKS_LIST, dateSeparator = 0, dateFormat = 0;
  bool saveSucceeds = true;
  bool saveToFile() { return saveSucceeds; }
  int getReaderFontId() { return 1; }
};
inline CrossPointSettings SETTINGS;
struct ButtonNavigator {
  using Buttons = std::initializer_list<MappedInputManager::Button>;
  template <class... T>
  ButtonNavigator(T&&...) {}
  ButtonNavigator() = default;
  template <class F>
  void onNextRelease(F) {}
  template <class F>
  void onPreviousRelease(F) {}
  template <class F>
  void onNextContinuous(F) {}
  template <class F>
  void onPreviousContinuous(F) {}
  template <class F>
  void onRelease(Buttons, F) {}
  template <class F>
  void onContinuous(Buttons, F) {}
  static Buttons getNextButtons() { return {}; }
  static int nextIndex(int i, int n) { return (i + 1) % n; }
  static int previousIndex(int i, int n) { return (i + n - 1) % n; }
  static int nextPageIndex(int i, int n, int p) { return std::min(i + p, n - 1); }
  static int previousPageIndex(int i, int, int p) { return std::max(i - p, 0); }
  void reset() {}
};

struct OptionPopup {
  template <class... T>
  void show(T&&...) {}
  template <class... T>
  void setDisabledOptions(T&&...) {}
  void setDismissOnOutsideTouchDown(bool) {}
  void setDividerAfterOption(int) {}
  template <class... T>
  bool handleInput(T&&...) {
    return false;
  }
  bool isActive() const { return false; }
  template <class... T>
  bool processInput(T&&...) {
    return false;
  }
  template <class... T>
  bool processRender(T&&...) {
    return false;
  }
};
struct UITheme {
  struct Metrics {
    int verticalSpacing = 8, contentSidePadding = 10, topPadding = 0, buttonHintsHeight = 30, batteryWidth = 30;
    bool headerBatteryDetached = false;
    int headerBatterySide = 1;
  };
  static UITheme& getInstance() {
    static UITheme t;
    return t;
  }
  Metrics getMetrics() { return {}; }
  static int getFileIcon(const std::string&) { return 0; }
  static std::string getCoverThumbPath(const std::string& p, int, int) { return p; }
  template <class... T>
  void drawButtonHints(T&&...) {}
  template <class... T>
  void drawHeader(T&&...) {}
  template <class... T>
  void drawPopup(T&&...) {}
};
inline auto& GUI = UITheme::getInstance();
inline auto makeUiTarget(GfxRenderer& r) { return freeink::ui::GfxRendererTarget(r, true); }
template <class... T>
void applySharedUiTheme(T&&...) {}
inline freeink::ui::InputSnapshot touchSnapshotFrom(MappedInputManager&) { return {}; }
struct TouchHeaderBackButton {
  struct Layout {
    freeink::ui::Rect touchRect;
  };
  static Layout layout(freeink::ui::Rect r) { return {r}; }
  static int height(UITheme::Metrics, MappedInputManager&) { return 44; }
  static freeink::ui::Rect headerRect(GfxRenderer&, MappedInputManager&) { return {0, 0, 480, 44}; }
  template <class... T>
  static void draw(T&&...) {}
  template <class... T>
  static bool wasTapped(T&&...) {
    return false;
  }
};
namespace RecentBookProgress {
inline float loadCachedEpubPercent(const RecentBook&) { return -1; }
inline float loadPercent(const RecentBook&) { return -1; }
inline bool hasPercent(float p) { return p >= 0; }
inline std::string formatPercent(float) { return "50%"; }
}  // namespace RecentBookProgress
struct BookReadingStats {
  bool isCompleted = false;
  static BookReadingStats load(const std::string&) { return {}; }
};
enum class BmpReaderError { Ok };
struct Bitmap {
  Bitmap(HalFile&) {}
  BmpReaderError parseHeaders() { return BmpReaderError::Ok; }
  int getWidth() { return 1; }
  int getHeight() { return 1; }
};
struct FileBrowserActionActivity : Activity {
  struct MenuItem {
    FileBrowserAction action;
    StrId labelId;
    const char* label = nullptr;
  };
  template <class... T>
  FileBrowserActionActivity(GfxRenderer& r, MappedInputManager& m, T&&...) : Activity("", r, m) {}
};
inline std::string confirmationSubject;
struct ConfirmationActivity : Activity {
  ConfirmationActivity(GfxRenderer& r, MappedInputManager& m, const std::string&, const std::string& subject,
                       bool = false)
      : Activity("", r, m) {
    confirmationSubject = subject;
  }
};
enum class InputType { Text };
struct KeyboardEntryActivity : Activity {
  template <class... T>
  KeyboardEntryActivity(GfxRenderer& r, MappedInputManager& m, T&&...) : Activity("", r, m) {}
};
struct OptionSelectionActivity : Activity {
  template <class... T>
  OptionSelectionActivity(GfxRenderer& r, MappedInputManager& m, T&&...) : Activity("", r, m) {}
};
struct EpubReaderActivity {
  static uint8_t loadBookRenderMode(const std::string&) { return 0; }
  static bool saveBookRenderMode(const std::string&, uint8_t) { return true; }
};
namespace BookActions {
inline std::vector<FileBrowserActionActivity::MenuItem> buildBookActionItems(const std::string&, bool) { return {}; }
inline std::vector<std::string> completedPaths;
inline bool isBookCompleted(const std::string& p) {
  return std::find(completedPaths.begin(), completedPaths.end(), p) != completedPaths.end();
}
inline bool canSendNearby(const std::string&) { return true; }
inline std::vector<std::string> clearedMetadata;
inline bool clearFileMetadata(const std::string& p) {
  clearedMetadata.push_back(p);
  return true;
}
inline bool deleteFile(const std::string& p) {
  if (!Storage.remove(p.c_str())) return false;
  return clearFileMetadata(p);
}
inline bool clearBookCache(const std::string&) { return true; }
inline bool deleteBookStats(const std::string&) { return true; }
inline bool resetBookReaderSettings(const std::string&) { return true; }
inline std::vector<std::string> epubRenderModeOptions() { return {}; }
inline uint8_t epubRenderModeDisplayIndex(uint8_t b) { return b; }
inline uint8_t epubRenderModeForDisplayIndex(uint8_t b) { return b; }
inline std::string confirmationHeading(StrId) { return ""; }
inline bool toggleBookCompleted(const std::string&, const std::string&, bool&) { return true; }
inline void drawToast(const GfxRenderer&, const char*) {}
}  // namespace BookActions
struct ActivityManager {
  void goToNearbyBookSend(const std::string&, bool) {}
};
inline ActivityManager activityManager;
namespace DateFormatting {
template <class... T>
std::string formatDate(T&&...) {
  return "date";
}
}  // namespace DateFormatting

inline freeink::ui::BitmapRef listIconFor(int, int) { return {}; }
enum class UiListRowType { WithSubtitle };
inline void configureUiList(freeink::ui::ListProps&, const freeink::ui::ThemeTokens&, freeink::ui::Rect,
                            UiListRowType) {}
struct Scale {
  int bodyFontId = 1;
};
inline Scale uiScaleSpec() { return {}; }
template <class... T>
void drawLucideIcon(T&&...) {}
