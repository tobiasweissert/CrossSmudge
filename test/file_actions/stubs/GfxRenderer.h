#pragma once
#include <EpdFontFamily.h>

#include <string>
#include <vector>
class GfxRenderer {
 public:
  inline static int width = 800, height = 480;
  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  int getLineHeight(int) const { return 20; }
  int getTextWidth(int, const char* t, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return int(std::string(t).size()) * 8;
  }
  int getSpaceWidth(int, EpdFontFamily::Style = EpdFontFamily::REGULAR) const { return 8; }
  std::vector<std::string> wrappedText(int, const char* t, int, int n, EpdFontFamily::Style) const {
    return n > 0 && t && *t ? std::vector<std::string>{t} : std::vector<std::string>{};
  }
  void displayBuffer() const {}
};
