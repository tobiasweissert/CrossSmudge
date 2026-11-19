#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Epub {
 public:
  bool optimizerImageAvailable = false;
  uint16_t optimizerImageWidth = 0, optimizerImageHeight = 0;
  mutable size_t streamReadCount = 0;
  bool getOptimizerImageDimensions(const std::string&, uint16_t& w, uint16_t& h) const { if (!optimizerImageAvailable) return false; w=optimizerImageWidth; h=optimizerImageHeight; return true; }
  bool extractItemToFile(const std::string&, const std::string&) const { return false; }
  static inline bool enableGifProbeForTest = false;
  std::vector<uint8_t> imageBytes;
  mutable size_t headerProbes = 0;
  mutable size_t fullExtractions = 0;
  template <typename Output>
  bool readItemContentsToStream(const std::string&, Output& output, size_t, bool allowEarlyStop = false) const {
    ++streamReadCount;
    if (imageBytes.empty()) return false;
    if (allowEarlyStop)
      ++headerProbes;
    else
      ++fullExtractions;
    const size_t written = output.write(imageBytes.data(), imageBytes.size());
    return written == imageBytes.size() || allowEarlyStop;
  }
};
