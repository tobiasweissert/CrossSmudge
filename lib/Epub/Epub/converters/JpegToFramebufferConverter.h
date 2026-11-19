#pragma once

#include <stdint.h>

#include <string>

#include "ImageToFramebufferDecoder.h"

class JpegToFramebufferConverter final : public ImageToFramebufferDecoder {
 public:
  static bool getDimensionsStatic(const std::string& imagePath, ImageDimensions& out);

  bool decodeToFramebuffer(const std::string& imagePath, GfxRenderer& renderer, const RenderConfig& config) override;
  // Reports successful cache writes separately from framebuffer rendering.
  bool decodeToFramebuffer(const std::string& imagePath, GfxRenderer& renderer, const RenderConfig& config,
                           bool& cacheWritten);

  bool decodeToCache(const std::string& imagePath, const RenderConfig& config) override {
    return decode(imagePath, nullptr, config);
  }
  bool decode(const std::string& imagePath, GfxRenderer* renderer, const RenderConfig& config);

  bool getDimensions(const std::string& imagePath, ImageDimensions& dims) const override {
    return getDimensionsStatic(imagePath, dims);
  }

  static bool supportsFormat(const std::string& extension);
  const char* getFormatName() const override { return "JPEG"; }
};
