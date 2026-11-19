#pragma once
#include <Arena.h>
#include <Print.h>

#include <algorithm>
#include <deque>
#include <optional>
#include <vector>

#include "Epub.h"
#include "expat.h"

class BookMetadataCache;

class ContentOpfParser final : public Print {
  enum ParserState {
    START,
    IN_PACKAGE,
    IN_METADATA,
    IN_BOOK_TITLE,
    IN_BOOK_AUTHOR,
    IN_BOOK_LANGUAGE,
    IN_BOOK_IDENTIFIER,
    IN_META_TEXT,
    IN_MANIFEST,
    IN_SPINE,
    IN_GUIDE,
  };

  const std::string& cachePath;
  const std::string& baseContentPath;
  size_t remainingSize;
  XML_Parser parser = nullptr;
  ParserState state = START;
  BookMetadataCache* cache;
  const bool metadataOnly;
  bool metadataComplete = false;
  HalFile tempItemStore;
  std::string coverItemId;
  bool hasExplicitStartReference = false;
  // XML character data is allowed to arrive in several callbacks for one text
  // node (notably around character references). Keep whitespace and creator
  // separation as element state rather than inferring either from callbacks.
  bool metadataSpacePending = false;
  bool authorSeparatorPending = false;

  std::string identifierText;
  std::string identifierScheme;
  std::string metaText;
  std::string metaProperty;
  std::string metaRefines;
  std::string metaId;
  struct CollectionMetadata {
    std::string id;
    std::string title;
    std::optional<float> index;
    bool isSeries = false;
  };
  std::vector<CollectionMetadata> collectionCandidates;
  std::string calibreSeries;
  std::optional<float> calibreSeriesIndex;

  // Index for fast idref→href lookup (binary search over .items.bin)
  Arena itemIndexArena;
  bool parseFailed = false;
  bool lowMemoryFailure = false;
  bool collectCssFiles = true;

  // Index for compact idref->href lookup. The temp manifest rows retain the
  // full ID for collision-safe matching without retaining IDs in heap memory.
  struct ItemIndexEntry {
    uint64_t idHash;      // FNV-1a hash of itemId
    uint16_t idLen;       // length for collision reduction
    uint32_t fileOffset;  // offset in .items.bin
  };
  static constexpr size_t ITEM_INDEX_CHUNK_CAPACITY = 240;
  struct ItemIndexChunk {
    ItemIndexChunk* next = nullptr;
    uint16_t count = 0;
    ItemIndexEntry entries[ITEM_INDEX_CHUNK_CAPACITY];
  };
  ItemIndexChunk* itemIndexHead = nullptr;
  ItemIndexChunk* itemIndexTail = nullptr;
  size_t itemIndexCount = 0;
  size_t itemIndexChunkCount = 0;

  bool appendItemIndexEntry(const ItemIndexEntry& entry);
  void sortItemIndexChunks();
  bool findItemHref(const std::string& idref, std::string& href);
  static bool itemIndexEntryLess(const ItemIndexEntry& lhs, const ItemIndexEntry& rhs) {
    return lhs.idHash < rhs.idHash || (lhs.idHash == rhs.idHash && lhs.idLen < rhs.idLen);
  }

  // FNV-1a hash function
  static uint64_t fnvHash(const char* s, size_t len) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < len; ++i) {
      hash ^= static_cast<uint8_t>(s[i]);
      hash *= 1099511628211ull;
    }
    return hash;
  }
  static uint64_t fnvHash(const std::string& s) { return fnvHash(s.c_str(), s.size()); }
  static uint64_t fnvHash(const char* s) {
    if (!s) return 0;
    uint64_t hash = 14695981039346656037ull;
    while (*s != '\0') {
      hash ^= static_cast<uint8_t>(*s);
      hash *= 1099511628211ull;
      ++s;
    }
    return hash;
  }

  static void startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void characterData(void* userData, const XML_Char* s, int len);
  static void endElement(void* userData, const XML_Char* name);

 public:
  std::string title;
  std::string author;
  std::string language;
  std::string isbn;
  std::string asin;
  std::string series;
  std::optional<float> seriesIndex;
  std::string tocNcxPath;
  std::string tocNavPath;        // EPUB 3 nav document path
  std::string guideTocPageHref;  // EPUB 2 guide TOC page, if declared
  std::string coverItemHref;
  std::string guideCoverPageHref;  // Guide reference with type="cover" or "cover-page" (points to XHTML wrapper)
  std::string textReferenceHref;
  std::vector<std::string> cssFiles;  // CSS stylesheet paths

  explicit ContentOpfParser(const std::string& cachePath, const std::string& baseContentPath, const size_t xmlSize,
                            BookMetadataCache* cache, const bool metadataOnly = false,
                            const bool collectCssFiles = true)
      : cachePath(cachePath),
        baseContentPath(baseContentPath),
        remainingSize(xmlSize),
        cache(cache),
        metadataOnly(metadataOnly),
        collectCssFiles(collectCssFiles) {}
  ~ContentOpfParser() override;

  bool setup();
  bool failedForLowMemory() const { return lowMemoryFailure; }

  size_t write(uint8_t) override;
  size_t write(const uint8_t* buffer, size_t size) override;
};
