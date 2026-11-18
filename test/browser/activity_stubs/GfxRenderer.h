#pragma once
#include <string>
#include <vector>
struct HalDisplay {
  enum RefreshMode { FULL_REFRESH, FAST_REFRESH };
};
struct Rect {
  int x, y, w, h;
};
struct EpdFontFamily {
  enum { BOLD };
};
class GfxRenderer {
 public:
  enum class Orientation { Portrait };
  Orientation getOrientation() { return Orientation::Portrait; }
  void setOrientation(Orientation) {}
  int getScreenWidth() { return 480; }
  int getScreenHeight() { return 800; }
  void clearScreen() {}
  void displayBuffer(HalDisplay::RefreshMode = HalDisplay::FAST_REFRESH) {}
  void drawCenteredText(int, int, const char*, bool = true, int = 0) {}
  std::vector<std::string> wrappedText(int, const char*, int, int) { return {}; }
  int getLineHeight(int) { return 20; }
};
