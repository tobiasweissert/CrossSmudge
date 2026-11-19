#pragma once

#include <PoolBudget.h>

#include <cstdint>

namespace MemoryBudget {
inline constexpr size_t JPEG_DECODER_APPROX_BYTES = 8192;
inline void logEpubHeapPools(const char*) {}
struct Snapshot {
  uint32_t freeHeap = UINT32_MAX;
  uint32_t maxAllocHeap = UINT32_MAX;
};
inline constexpr uint32_t EPUB_TEXT_LAYOUT_MIN_FREE = 44U * 1024U;
inline bool textHeapAvailable = true;
inline Snapshot currentHeap{};
inline Snapshot snapshot() { return currentHeap; }
inline bool hasHeapForEpubTextLayoutStart(Snapshot heap) {
  return textHeapAvailable && heap.freeHeap >= EPUB_TEXT_LAYOUT_MIN_FREE;
}
inline bool hasHeap(Snapshot heap, uint32_t free, uint32_t largest) {
  return heap.freeHeap >= free && heap.maxAllocHeap >= largest;
}
inline bool shouldReleaseSdFontCachesForEpubInlineImage(Snapshot) { return false; }
inline bool hasHeapForEpubInlineImage(const char*, const char*) { return true; }
inline bool hasHeapForOptimizerPxcImage(const char*, const char*) { return true; }
}  // namespace MemoryBudget
