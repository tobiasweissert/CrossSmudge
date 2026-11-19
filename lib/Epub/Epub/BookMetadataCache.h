#pragma once

#include <Arena.h>
#include <ArenaVector.h>
#include <BufferedFile.h>
#include <HalStorage.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <vector>

class BookMetadataCache {
 public:
  struct BookMetadata {
    std::string title;
    std::string author;
    std::string language;
    std::string coverItemHref;
    std::string textReferenceHref;
  };

  struct SpineEntry {
    std::string href;
    uint32_t cumulativeSize;  // cumulative size stored as 32-bit for on-disk format compatibility
    int16_t tocIndex;

    SpineEntry() : cumulativeSize(0), tocIndex(-1) {}
    SpineEntry(std::string href, const uint32_t cumulativeSize, const int16_t tocIndex)
        : href(std::move(href)), cumulativeSize(cumulativeSize), tocIndex(tocIndex) {}
  };

  struct TocEntry {
    std::string title;
    std::string href;
    std::string anchor;
    uint8_t level;
    int16_t spineIndex;

    TocEntry() : level(0), spineIndex(-1) {}
    TocEntry(std::string title, std::string href, std::string anchor, const uint8_t level, const int16_t spineIndex)
        : title(std::move(title)),
          href(std::move(href)),
          anchor(std::move(anchor)),
          level(level),
          spineIndex(spineIndex) {}
  };

 private:
  std::string cachePath;
  uint32_t lutOffset;
  uint16_t spineCount;
  uint16_t tocCount;
  bool loaded;
  bool buildMode;
  bool cacheCumulativeSizes;

  // Optional reader-session cache. Lightweight metadata-only callers leave this
  // disabled so Recent Books does not pay steady-state RAM for a few lookups.
  std::unique_ptr<uint32_t[]> cumulativeSizes;
  uint16_t cumulativeSizeCount = 0;

  HalFile bookFile;
  // Temp file handles during build
  HalFile spineFile;
  HalFile tocFile;
  // Buffers the per-entry tmp-file writes during the OPF/TOC passes: those
  // writes interleave with zip-inflate SD reads, and unbuffered they thrash
  // SdFat's shared sector cache (one 512B transaction per 4-byte pod). One
  // wrapper serves whichever pass is active (spine, then toc).
  std::unique_ptr<serialization::BufferedFileWriter> passOut;

  // Cumulative spine sizes stay in book.bin; RAM holds every SIZE_WINDOW-th one and one window.
  static constexpr int SIZE_WINDOW = 32;
  std::vector<uint32_t> windowStarts;  // total before item w * SIZE_WINDOW
  uint32_t bookSize = 0;
  mutable int windowFirst = -1;
  mutable uint32_t window[SIZE_WINDOW] = {};
  bool fillWindow(int first) const;  // caller holds sizeWindowLock

  // Index for fast href→spineIndex lookup. Exists only during a large EPUB's
  // TOC pass; a bare deque member would hold its first node for every open book.
  struct SpineHrefIndexEntry {
    uint64_t hrefHash;  // FNV-1a 64-bit hash
    uint16_t hrefLen;   // length for collision reduction
    int16_t spineIndex;
  };
  Arena spineHrefIndexArena;
  ArenaVector<SpineHrefIndexEntry> spineHrefIndex;
  bool useSpineHrefIndex = false;
  bool lowMemoryFailure = false;

  static constexpr uint16_t LARGE_SPINE_THRESHOLD = 300;

  // FNV-1a 64-bit hash function
  static uint64_t fnvHash64(const std::string& s) {
    uint64_t hash = 14695981039346656037ull;
    for (char c : s) {
      hash ^= static_cast<uint8_t>(c);
      hash *= 1099511628211ull;
    }
    return hash;
  }

  uint32_t writeSpineEntry(HalFile& file, const SpineEntry& entry) const;
  uint32_t writeTocEntry(HalFile& file, const TocEntry& entry) const;
  SpineEntry readSpineEntry(HalFile& file) const;
  TocEntry readTocEntry(HalFile& file) const;
  void cacheSpineCumulativeSizes();

 public:
  BookMetadata coreMetadata;

  explicit BookMetadataCache(std::string cachePath, const bool cacheCumulativeSizes = false)
      : cachePath(std::move(cachePath)),
        lutOffset(0),
        spineCount(0),
        tocCount(0),
        loaded(false),
        buildMode(false),
        cacheCumulativeSizes(cacheCumulativeSizes),
        spineHrefIndex(spineHrefIndexArena) {}
  ~BookMetadataCache() = default;

  // Building phase (stream to disk immediately)
  bool beginWrite();
  bool beginContentOpfPass();
  void createSpineEntry(const std::string& href);
  bool endContentOpfPass();
  bool beginTocPass();
  void createTocEntry(const std::string& title, const std::string& href, const std::string& anchor, uint8_t level);
  bool endTocPass();
  bool endWrite();
  bool cleanupTmpFiles() const;

  // Post-processing to update mappings and sizes
  bool buildBookBin(const std::string& epubPath, const BookMetadata& metadata);

  // Cheap check (no parsing) for whether a metadata cache exists at cachePath.
  // Lets callers predict a fast cached open without doing the full load().
  static bool exists(const std::string& cachePath);

  // Reading phase (read mode)
  bool load();
  bool failedForLowMemory() const { return lowMemoryFailure; }
  SpineEntry getSpineEntry(int index);
  size_t getSpineCumulativeSize(int index);
  TocEntry getTocEntry(int index);
  // Cumulative byte size up to and including the given spine item (0 if out of range
  // or not loaded).
  uint32_t getCumulativeSize(int index) const;
  // First spine index whose cumulative size reaches `size` (-1 past the end or not loaded). Reads at most one window.
  int getSpineIndexForSize(uint32_t size) const;
  int getSpineCount() const { return spineCount; }
  int getTocCount() const { return tocCount; }
  bool isLoaded() const { return loaded; }
};
