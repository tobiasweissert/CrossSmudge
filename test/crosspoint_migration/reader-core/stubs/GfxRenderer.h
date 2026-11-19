#pragma once

#include <cstdint>
#include <vector>

class GfxRenderer {
 public:
  explicit GfxRenderer(int w = 800, int h = 480) : width(w), height(h), framebuffer((w + 7) / 8 * h, 0xff) {}
  enum RenderMode { BW, GRAYSCALE_MSB, GRAYSCALE_LSB };
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

  bool grayPlanesAreAbsolute() const { return false; }
  bool imagesDitheredInBw() const { return false; }
  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  int getDisplayWidth() const { return width; }
  int getDisplayHeight() const { return height; }
  uint16_t getDisplayWidthBytes() const { return static_cast<uint16_t>((width + 7) / 8); }
  uint8_t* getWriteTarget() const { return framebuffer.data(); }
  int getWriteOriginY() const { return 0; }
  int getWriteRows() const { return height; }
  RenderMode getRenderMode() const { return BW; }
  Orientation getOrientation() const { return LandscapeCounterClockwise; }

 private:
  int width;
  int height;
  mutable std::vector<uint8_t> framebuffer;
};
