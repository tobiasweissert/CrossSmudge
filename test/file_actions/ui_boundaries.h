// Geometry-only HAL boundaries. Production screen builder/callbacks are not replaced.
struct Rect {
  int x = 0, y = 0, width = 480, height = 800;
};
namespace freeink {
namespace ui {
using ActionId = int;
struct Rect {
  int16_t x = 0, y = 0, width = 480, height = 800;
};
struct Insets {
  int16_t top = 0, right = 0, bottom = 0, left = 0;
};
enum Color { Black };
struct Paint {
  static Paint solid(Color) { return {}; }
};
inline int plainStyles(Paint = {}) { return 0; }
struct Font {};
struct TextStyle {
  int maxLines = 1;
  bool bold = false;
  Font font;
};
struct Bitmap {
  int16_t width = 24, height = 24;
};
inline Bitmap bitmapFromIcon(int) { return {}; }
enum class BitmapMode { Center };
struct ActionEvent {
  int value = 0;
  bool longPress = false;
};
struct InputSnapshot {
  bool touchPressed = false, touchReleased = false;
  int x = 0, y = 0;
};
constexpr int InputTouch = 1, InputLongPress = 2;
struct ButtonProps {
  const char* label = nullptr;
  TextStyle text;
  ActionId action = 0;
  int styles = 0, minTouchSize = 0, inputMask = 0;
};
struct ListItem {
  const char* label = nullptr;
  const char* value = nullptr;
  Bitmap icon;
  int16_t actionValue = 0;
};
struct ListProps {
  TextStyle labelText;
  int rowHeight = 48, rowGap = 4;
  const ListItem* items = nullptr;
  uint16_t count = 0, itemsWindowFirst = 0, itemsWindowCount = 0;
  int16_t selectedIndex = 0, topIndex = 0;
  void* nav = nullptr;
  ActionId action = 0;
  uint16_t inputMask = 0;
  int valueInset = 0;
  bool balanceWrappedLabelWithValue = false, partialTrailingRow = false;
};
struct Target {
  void bitmap(Rect, Bitmap, BitmapMode, Paint) {}
  void fill(Rect, Paint) {}
};
struct Theme {
  TextStyle bodyText;
  int16_t minTouchSize = 48, listScrollWidth = 4, listScrollSide = 0, listScrollInset = 0;
};
struct Screen {
  Rect area;
  Theme style;
  Target output;
  struct Button {
    ButtonProps props;
    Rect rect;
  };
  std::vector<Button> buttons;
  explicit Screen(int w = 480, int h = 800) { area = {0, 0, int16_t(w), int16_t(h)}; }
  void setContentMargin(Insets i) {
    area.x += i.left;
    area.y += i.top;
    area.width -= i.left + i.right;
    area.height -= i.top + i.bottom;
  }
  Theme& theme() { return style; }
  Target& target() { return output; }
  Rect body() const { return area; }
  void spacer(int16_t n) {
    area.y += n;
    area.height -= n;
  }
  Rect takeBottom(int16_t n) {
    area.height -= n;
    return {area.x, int16_t(area.y + area.height), area.width, n};
  }
  void button(ButtonProps p, Rect r) { buttons.push_back({p, r}); }
  void centeredText(const char*, TextStyle) {}
  void list(const ListProps&) {}
};
inline void drawListScrollIndicator(Target&, Rect, size_t, uint16_t, int, int, int, int) {}
}  // namespace ui
}  // namespace freeink
namespace fui = freeink::ui;
struct UiApp {
  using ScreenType = fui::Screen;
};
struct Metrics {
  int topPadding = 0, buttonHintsHeight = 0, verticalSpacing = 12, contentSidePadding = 16;
};
struct UITheme {
  static UITheme& getInstance() {
    static UITheme t;
    return t;
  }
  const Metrics& getMetrics() {
    static Metrics m;
    return m;
  }
  static int getFileIcon(const std::string&) { return 0; }
  UITheme& getTheme() { return *this; }
  void drawHeader(const GfxRenderer&, Rect, const char*) {}
  void drawButtonHints(const GfxRenderer&, const char*, const char*, const char*, const char*) {}
  void drawPopup(const GfxRenderer&, const char*) {}
};
#define GUI UITheme::getInstance()
struct TouchHeaderBackButton {
  inline static std::string title;
  static int height(const Metrics&, const MappedInputManager&) { return 48; }
  static Rect headerRect(const GfxRenderer& r, const MappedInputManager&) { return {0, 0, r.getScreenWidth(), 48}; }
  struct Layout {
    Rect iconRect;
  };
  static Layout layout(Rect) { return {{0, 0, 48, 48}}; }
  static constexpr int TITLE_VERTICAL_OFFSET = 0;
  static bool wasTapped(MappedInputManager&, GfxRenderer&) { return false; }
  static void draw(GfxRenderer&, int, Rect, const char* t, bool, int) { title = t; }
};
constexpr int SMALL_FONT_ID = 0, icon_sliders_horizontal_24 = 0;
enum class UiListRowType { WithSubtitle, SingleLine };
inline int configureUiList(fui::ListProps&, fui::Theme&, fui::Rect r, UiListRowType) { return r.height / 52; }
inline bool usesTwoLineFileBrowserRows() { return false; }
inline fui::Bitmap listIconFor(int, int) { return {}; }
struct {
  int hideFileExtension = 0;
} SETTINGS;
class ButtonNavigator {
 public:
  template <class T>
  void onRelease(std::initializer_list<MappedInputManager::Button>, T) {}
  template <class T>
  void onContinuous(std::initializer_list<MappedInputManager::Button>, T) {}
  template <class T>
  void onNextRelease(T) {}
  template <class T>
  void onPreviousRelease(T) {}
  template <class T>
  void onNextContinuous(T) {}
  template <class T>
  void onPreviousContinuous(T) {}
  static int nextIndex(int x, int n) { return (x + 1) % n; }
  static int previousIndex(int x, int n) { return (x + n - 1) % n; }
  static int nextPageIndex(int x, int n, int) { return nextIndex(x, n); }
  static int previousPageIndex(int x, int n, int) { return previousIndex(x, n); }
};
inline fui::InputSnapshot touchSnapshotFrom(MappedInputManager& i) { return {i.down, i.tap, i.x, i.y}; }
inline bool isSupportedBrowserFile(const std::string& p) {
  return FsHelpers::hasEpubExtension(p) || FsHelpers::hasPngExtension(p);
}
