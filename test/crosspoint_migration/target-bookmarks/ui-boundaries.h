#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>

#include "BookmarkStore.h"
#include "activities/ActivityResult.h"
#include "components/themes/BaseTheme.h"

// Explicit application/HAL transport boundaries, not SDK substitutes.
struct MappedInputManager {
  enum class Button { Back, Confirm, NavNext, NavPrevious };
  enum class SwipeDir { None, Up, Down };
  bool hasTouchHardware() const { return true; }
  bool isPressed(Button) const { return false; }
  mutable Button release = Button::Back;
  bool released = false;
  bool wasReleased(Button button) const { return released && button == release; }
  bool wasPressed(Button) const { return false; }
  unsigned long getHeldTime() const { return 0; }
  SwipeDir wasSwipe() const { return SwipeDir::None; }
  const char* withBackArrow(const char* s) { return s; }
  struct Labels {
    const char *btn1, *btn2, *btn3, *btn4;
  };
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) { return {a, b, c, d}; }
};
struct RenderLock {};
struct Activity {
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  ActivityResult result{};
  bool finished = false;
  Activity(const char*, GfxRenderer& r, MappedInputManager& m) : renderer(r), mappedInput(m) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool isReaderActivity() const { return false; }
  virtual bool allowPowerAsConfirmInReaderMode() const { return false; }
  void setResult(ActivityResult r) { result = std::move(r); }
  template <class T>
  void setResult(T r) {
    result.data = std::move(r);
  }
  void finish() { finished = true; }
  void requestUpdate() {}
};
struct OptionPopup {
  template <class... T>
  void show(T&&...) {}
  void setPrimaryOptionIndex(int) {}
  template <class F>
  bool handleInput(MappedInputManager&, F) {
    return false;
  }
  bool processRender(GfxRenderer&, MappedInputManager&) { return false; }
};
namespace ReaderUtils {
constexpr int DELETE_HOLD_MS = 700;
}
#define tr(x) #x
struct BookmarkTheme {
  struct Metrics {
    int topPadding = 5, verticalSpacing = 8;
  } metrics;
  static BookmarkTheme& getInstance() {
    static BookmarkTheme t;
    return t;
  }
  const Metrics& getMetrics() const { return metrics; }
  Rect getScreenSafeArea(const GfxRenderer&, bool, bool) const { return Rect{0, 0, 480, 800}; }
  template <class... T>
  void drawHeader(T&&...) {}
  template <class... T>
  void drawButtonHints(T&&...) {}
};
#define UITheme BookmarkTheme
#define GUI BookmarkTheme::getInstance()
struct TouchHeaderBackButton {
  template <class T>
  static int height(const T&, MappedInputManager&) {
    return 40;
  }
  static bool wasTapped(MappedInputManager&, Rect) { return false; }
  template <class... T>
  static void draw(T&&...) {}
};
inline freeink::ui::GfxRendererTarget makeUiTarget(GfxRenderer& r) { return freeink::ui::GfxRendererTarget(r, true); }
template <class... T>
void applySharedUiTheme(T&&...) {}
inline freeink::ui::InputSnapshot touchSnapshotFrom(MappedInputManager&) { return {}; }
inline int scrollListBy(int top, int delta, int rows, int count) {
  return std::clamp(top + delta, 0, std::max(0, count - rows));
}
inline int followListSelection(int selected, int top, int rows, int count) {
  return scrollListBy(selected < top ? selected : (selected >= top + rows ? selected - rows + 1 : top), 0, rows, count);
}
enum class UiListRowType { WithSubtitle };
inline int configureUiList(freeink::ui::ListProps&, const freeink::ui::ThemeTokens&, freeink::ui::Rect, UiListRowType) {
  return 4;
}
