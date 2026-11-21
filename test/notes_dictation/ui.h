#pragma once
#include <memory>
#include <variant>
#include <vector>

#include "host.h"
struct KeyboardResult {
  std::string text;
};
struct OptionSelectionResult {
  uint8_t index;
};
struct WifiResult {
  bool connected;
  std::string ssid, ip;
};
struct ActivityResult {
  bool isCancelled = false;
  std::variant<std::monostate, WifiResult, KeyboardResult, OptionSelectionResult> data;
};
#include <I18n.h>
struct RenderLock {};
struct GfxRenderer {
  std::vector<std::string> labels;
  int w = 480, h = 800;
  int getScreenWidth() { return w; }
  int getScreenHeight() { return h; }
  int getLineHeight(int) { return 20; }
  void clearScreen() { labels.clear(); }
  void drawCenteredText(int, int, const char* s) { labels.push_back(s); }
  std::vector<std::string> wrappedText(int, const char* s, int, int) { return {s}; }
  void displayBuffer() {};
};
struct MappedInputManager {
  enum class Button { Back, Confirm };
  bool back = false, confirm = false, press = false, touch = false, held = false;
  bool wasPressed(Button) { return press; }
  bool isPressed(Button) { return held; }
  bool isScreenContactActive() { return touch; }
  bool wasScreenTouchDown(int&, int& y) {
    y = 650;
    return touch;
  }
  bool tap = false;
  bool wasScreenTapped(int&, int& y) {
    y = 650;
    bool v = tap;
    tap = false;
    return v;
  }
  bool wasReleased(Button b) {
    bool& v = b == Button::Back ? back : confirm;
    bool old = v;
    v = false;
    return old;
  }
};
inline constexpr int SMALL_FONT_ID = 1, UI_12_FONT_ID = 2;
enum class InputType { Password, Text };
struct Activity {
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  ActivityResult result;
  bool finished = false;
  static inline GfxRenderer defaultRenderer;
  static inline MappedInputManager defaultInput;
  Activity(const char* = "fixture", GfxRenderer& r = defaultRenderer, MappedInputManager& m = defaultInput)
      : renderer(r), mappedInput(m) {}
  virtual ~Activity() = default;
  virtual void render(RenderLock&&) {}
  virtual void loop() {}
  virtual void onInputLockChanged(bool) {}
  virtual void onFrontlightPanelOpened() {}
  virtual bool allowFrontlightPanelGesture() const { return true; }
  virtual bool handleHomeGesture() { return false; }
  virtual void prepareForSleep() {}
  virtual void onExit() {}
  void requestUpdate() {}
  void finish() { finished = true; }
  void setResult(KeyboardResult k) { result.data = std::move(k); }
  void setResult(ActivityResult r) { result = std::move(r); }
  using Handler = std::function<void(const ActivityResult&)>;
  std::unique_ptr<Activity> child;
  Handler cb;
  void startActivityForResult(std::unique_ptr<Activity>&& c, Handler h) {
    child = std::move(c);
    cb = std::move(h);
  }
  void complete(ActivityResult r) {
    child.reset();
    auto h = std::move(cb);
    h(r);
  }
};
struct WifiSelectionActivity : Activity {
  bool tearDownWifiOnExit = false;
  std::string selectedSSID = "fixture", connectedIP = "fixture";
  WifiSelectionActivity(GfxRenderer&, MappedInputManager&, bool = false) {}
  void onComplete(bool);
  void onExit() override;
};
struct QrDisplayActivity : Activity {
  std::string payload;
  QrDisplayActivity(GfxRenderer&, MappedInputManager&, std::string v) : payload(v) {}
};
struct OptionSelectionActivity : Activity {
  std::vector<std::string> labels;
  OptionSelectionActivity(GfxRenderer&, MappedInputManager&, const char*, StrId, const std::vector<std::string>& l, int,
                          bool, bool)
      : labels(l) {}
};
struct KeyboardEntryActivity : Activity {
  KeyboardEntryActivity(GfxRenderer&, MappedInputManager&, const char*, std::string initial, size_t max, InputType t,
                        int) {
    if (t == InputType::Password)
      assert(initial.empty() && max == 256);
    else
      assert(initial == "Neutral" && max == 3072);
  }
};
struct ConfirmationActivity : Activity {
  ConfirmationActivity(GfxRenderer&, MappedInputManager&, const char*, const char*) {}
};
