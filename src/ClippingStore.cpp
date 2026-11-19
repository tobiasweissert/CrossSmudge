#include "ClippingStore.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>
#include <esp_random.h>
#include <uzlib.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>

#include "clippings/ClippingPreview.h"
#include "network/ProtectedPaths.h"

namespace {
constexpr uint8_t LEGACY_VERSION = 1;
constexpr uint8_t TEXT_OFFSET_VERSION = 2;
constexpr uint8_t LAYOUT_SIGNATURE_VERSION = 3;
constexpr uint8_t TABLE_VERSION = 4;
constexpr uint8_t VERSION = 5;
constexpr size_t INITIAL_CLIPPING_RESERVE = 4;
constexpr char CLIPPINGS_DIR[] = "/.crosspoint/clippings";
constexpr size_t TEXT_COPY_BUFFER_SIZE = 128;
constexpr char MOVE_PENDING[] = "/.crosspoint/clippings/move.pending";

struct ClippingFileHeader {
  std::string title;
  std::string author;
  std::string path;
  std::string bookType;
  uint16_t count = 0;
};

std::string storeFilePathForBook(const std::string& filePath, const std::string& bookType) {
  const uint32_t crc = uzlib_crc32(filePath.data(), static_cast<unsigned int>(filePath.size()), 0);
  return std::string(CLIPPINGS_DIR) + "/" + bookType + "_" + std::to_string(crc) + ".bin";
}

std::string existingStorePathForBook(const std::string& path, const std::string& type) {
  const auto owner = storeFilePathForBook(path, type);
  const auto modern =
      std::string(CLIPPINGS_DIR) + "/" + type + "_" + std::to_string(std::hash<std::string>{}(path)) + ".bin";
  if (Storage.exists(owner.c_str()) || Storage.exists((owner + ".bak").c_str()) ||
      Storage.exists((owner + ".tmp").c_str()))
    return owner;
  for (const char* suffix : {"", ".bak", ".tmp", ".rename.bak", ".migrate.bak"})
    if (Storage.exists((modern + suffix).c_str())) return modern;
  return owner;
}

void copyBounded(char* dst, const size_t dstSize, const char* src) {
  if (dstSize == 0) return;
  if (!src) src = "";
  size_t length = 0;
  while (length < dstSize - 1 && src[length]) ++length;
  // If the first excluded byte continues a codepoint, exclude its lead
  // and any already copied continuation bytes as well.
  while (length > 0 && (static_cast<unsigned char>(src[length]) & 0xC0) == 0x80) --length;
  memcpy(dst, src, length);
  dst[length] = '\0';
}

bool readClippingFileHeader(const std::string& fullPath, const char* name, ClippingFileHeader& header) {
  HalFile f;
  if (!Storage.openFileForRead("CLIP", fullPath, f)) {
    return false;
  }

  uint8_t version = 0;
  uint16_t count = 0;
  if (!serialization::tryReadPod(f, version) ||
      (version != LEGACY_VERSION && version != TEXT_OFFSET_VERSION && version != LAYOUT_SIGNATURE_VERSION &&
       version != TABLE_VERSION && version != VERSION) ||
      !serialization::tryReadPod(f, count) || !serialization::tryReadString(f, header.title, CLIPPING_TEXT_MAX) ||
      !serialization::tryReadString(f, header.author, CLIPPING_TEXT_MAX) ||
      !serialization::tryReadString(f, header.path, CLIPPING_TEXT_MAX)) {
    f.close();
    return false;
  }
  if (!f.close()) return false;

  if (count > CLIPPING_MAX_PER_BOOK) {
    return false;
  }
  header.count = count;
  header.bookType = "epub";
  const std::string nameStr = name ? name : "";
  const size_t underscorePos = nameStr.find('_');
  if (underscorePos != std::string::npos) {
    header.bookType = nameStr.substr(0, underscorePos);
  }
  return true;
}

bool copyBytes(HalFile& in, HalFile& out, uint16_t length) {
  std::array<uint8_t, TEXT_COPY_BUFFER_SIZE> buffer{};
  while (length > 0) {
    const size_t chunk = std::min<size_t>(length, buffer.size());
    if (in.read(buffer.data(), chunk) != static_cast<int>(chunk)) {
      return false;
    }
    if (out.write(buffer.data(), chunk) != chunk) {
      return false;
    }
    length = static_cast<uint16_t>(length - chunk);
  }
  return true;
}
}  // namespace

ClippingStore ClippingStore::instance;

bool ClippingStore::loadForBook(const std::string& filePath, const std::string& title, const std::string& author,
                                const std::string& bookType) {
  return loadForBookImpl(filePath, title, author, bookType, false);
}

bool ClippingStore::loadForBookImpl(const std::string& filePath, const std::string& title, const std::string& author,
                                    const std::string& bookType, const bool moveOwner) {
  loaded = false;
  clippings.clear();
  if (!moveOwner && Storage.exists(MOVE_PENDING)) return false;
  if (bookType != "epub" || filePath.empty() || filePath[0] != '/' || title.size() > CLIPPING_TEXT_MAX ||
      author.size() > CLIPPING_TEXT_MAX || filePath.size() > CLIPPING_TEXT_MAX) {
    LOG_ERR("CLIP", "Unknown clipping book type: %s", bookType.c_str());
    return false;
  }

  loaded = false;
  deletedIds.clear();
  bookFilePath = filePath;
  bookTitle = title;
  bookAuthor = author;
  dirty = false;
  clippings.clear();
  if (clippings.capacity() < INITIAL_CLIPPING_RESERVE) {
    clippings.reserve(INITIAL_CLIPPING_RESERVE);
  }

  storeFilePath = existingStorePathForBook(filePath, bookType);
  // Retained publication roots are durable uncertainty, never empty new data.
  for (const char* suffix : {".tmp", ".bak", ".rename.bak", ".migrate.bak"})
    if (Storage.exists((storeFilePath + suffix).c_str())) return false;
  if (!Storage.exists(storeFilePath.c_str())) return loaded = true;
  ClippingFileHeader identity;
  if (!readClippingFileHeader(storeFilePath, "epub_", identity) || identity.path != filePath) return false;
  loaded = readFromFile();
  if (!loaded) {
    clippings.clear();
    return false;
  }
  // Import old upstream journals read-only; never truncate an interrupted tail.
  const std::string journalPath = storeFilePath + ".deleted";
  HalFile versionFile;
  uint8_t diskVersion = 0;
  if (!Storage.openFileForRead("CLIP", storeFilePath, versionFile) ||
      !serialization::tryReadPod(versionFile, diskVersion) || !versionFile.close())
    return loaded = false;
  if (diskVersion != VERSION && Storage.exists(journalPath.c_str())) {
    HalFile journal;
    if (!Storage.openFileForRead("CLIP", journalPath, journal) || journal.size() % 65 ||
        journal.size() / 65 > CLIPPING_MAX_PER_BOOK)
      return loaded = false;
    while (journal.position() < journal.size()) {
      char id[65];
      if (journal.read(reinterpret_cast<uint8_t*>(id), sizeof(id)) != sizeof(id) || !memchr(id, 0, sizeof(id)) ||
          !id[0])
        return loaded = false;
      if (std::find(deletedIds.begin(), deletedIds.end(), id) == deletedIds.end()) deletedIds.emplace_back(id);
    }
    if (!journal.close() || deletedIds.size() > CLIPPING_MAX_PER_BOOK) return loaded = false;
    // Persist import before acknowledging any remote operation.
    dirty = true;
  }
  return true;
}

void ClippingStore::unload() {
  if (dirty && loaded) saveToFile();
  loaded = false;
  deletedIds.clear();
  clippings.clear();
  bookFilePath.clear();
  bookTitle.clear();
  bookAuthor.clear();
  storeFilePath.clear();
  dirty = false;
}

ClippingStore::AddResult ClippingStore::addClipping(const uint16_t spineIndex, const uint16_t startPage,
                                                    const uint16_t endPage, const uint16_t pageCount,
                                                    const uint16_t startWordIndex, const uint16_t endWordIndex,
                                                    const uint16_t wordCount, const char* chapterTitle,
                                                    const uint16_t paragraphIndex, const std::string& text,
                                                    const uint16_t tableSelection, const uint32_t layoutSignature) {
  if (!loaded || text.size() > CLIPPING_TEXT_MAX) return AddResult::SaveFailed;
  if (clippings.size() >= CLIPPING_MAX_PER_BOOK) {
    LOG_ERR("CLIP", "Clipping limit (%u) reached", CLIPPING_MAX_PER_BOOK);
    return AddResult::LimitReached;
  }

  Clipping clipping;
  clipping.spineIndex = spineIndex;
  clipping.startPage = startPage;
  clipping.endPage = endPage;
  clipping.pageCount = std::max<uint16_t>(1, pageCount);
  clipping.startWordIndex = startWordIndex;
  clipping.endWordIndex = endWordIndex;
  clipping.wordCount = wordCount;
  clipping.paragraphIndex = paragraphIndex;
  clipping.timestamp = static_cast<uint32_t>(millis() / 1000UL);
  clipping.layoutSignature = layoutSignature;
  clipping.tableSelection = tableSelection;
  copyBounded(clipping.chapterTitle, sizeof(clipping.chapterTitle), chapterTitle);
  clipping.textLength = static_cast<uint16_t>(std::min(text.size(), CLIPPING_TEXT_MAX));

  clippings.push_back(std::move(clipping));
  dirty = true;
  if (!writeToFile(&text, clippings.size() - 1)) {
    clippings.pop_back();
    dirty = true;
    return AddResult::SaveFailed;
  }
  dirty = false;
  return AddResult::Added;
}

bool ClippingStore::removeClippingAt(const size_t index) {
  if (!loaded || index >= clippings.size()) return false;
  if (clippings[index].id[0] && deletedIds.size() >= CLIPPING_MAX_PER_BOOK) return false;
  const size_t oldDeletionCount = deletedIds.size();
  if (clippings[index].id[0]) deletedIds.emplace_back(clippings[index].id);
  Clipping clipping = std::move(clippings[index]);
  clippings.erase(clippings.begin() + index);
  dirty = true;
  if (!saveToFile()) {
    deletedIds.resize(oldDeletionCount);
    clippings.insert(clippings.begin() + index, std::move(clipping));
    dirty = true;
    return false;
  }
  return true;
}

const Clipping* ClippingStore::clippingAt(const size_t index) const {
  if (index >= clippings.size()) return nullptr;
  return &clippings[index];
}

bool ClippingStore::cacheResolvedLayoutRange(const size_t index, const uint16_t page, const uint16_t startWord,
                                             const uint16_t endWord, const uint32_t layoutSignature) {
  if (index >= clippings.size()) return false;
  if (!cacheClippingResolvedLayoutRange(clippings[index], page, startWord, endWord, layoutSignature)) {
    return false;
  }
  dirty = true;
  return true;
}

bool ClippingStore::readClippingPreview(const size_t index, std::string& out) const {
  out.clear();
  const Clipping* clipping = clippingAt(index);
  if (!clipping || storeFilePath.empty()) {
    LOG_ERR("CLIP", "Invalid clipping preview index: %u", static_cast<unsigned>(index));
    return false;
  }
  if (clipping->textLength == 0) return true;

  HalFile f;
  if (!Storage.openFileForRead("CLIP", storeFilePath, f)) return false;
  if (!f.seek(clipping->textOffset)) {
    f.close();
    LOG_ERR("CLIP", "Failed to seek clipping preview at %u", clipping->textOffset);
    return false;
  }
  const bool ok = clippingPreview::read(f, clipping->textLength, out);
  f.close();
  if (!ok) LOG_ERR("CLIP", "Failed to read clipping preview at %u", clipping->textOffset);
  return ok;
}

bool ClippingStore::readClippingText(const size_t index, std::string& out) const {
  const Clipping* clipping = clippingAt(index);
  if (!clipping) return false;
  return readClippingText(*clipping, out);
}

bool ClippingStore::readClippingText(const Clipping& clipping, std::string& out) const {
  out.clear();
  if (clipping.textLength == 0) return true;
  if (storeFilePath.empty()) return false;

  HalFile f;
  if (!Storage.openFileForRead("CLIP", storeFilePath, f)) {
    return false;
  }
  if (!f.seek(clipping.textOffset)) {
    f.close();
    LOG_ERR("CLIP", "Failed to seek clipping text at %u: %s", clipping.textOffset, storeFilePath.c_str());
    return false;
  }
  out.resize(clipping.textLength);
  const int expected = static_cast<int>(clipping.textLength);
  const bool readOk = f.read(&out[0], clipping.textLength) == expected;
  const bool closed = f.close();
  const bool ok = readOk && closed;
  if (!ok) {
    out.clear();
    LOG_ERR("CLIP", "Failed to read clipping text at %u: %s", clipping.textOffset, storeFilePath.c_str());
  }
  return ok;
}

bool ClippingStore::saveToFile() {
  if (!loaded) return false;
  if (!dirty) return true;
  if (writeToFile()) {
    dirty = false;
    return true;
  }
  return false;
}

void ClippingStore::clearAll() {
  if (!loaded) return;
  const auto old = clippings;
  const auto oldDeleted = deletedIds;
  for (const auto& c : clippings) {
    if (c.id[0] && std::find(deletedIds.begin(), deletedIds.end(), c.id) == deletedIds.end())
      deletedIds.emplace_back(c.id);
  }
  if (deletedIds.size() > CLIPPING_MAX_PER_BOOK) {
    deletedIds = oldDeleted;
    return;
  }
  clippings.clear();
  dirty = true;
  if (!saveToFile()) {
    clippings = old;
    deletedIds = oldDeleted;
  }
}

bool ClippingStore::readFromFile() { return readFromFile(storeFilePath, clippings, &deletedIds); }

bool ClippingStore::readFromFile(const std::string& path, std::vector<Clipping>& out,
                                 std::vector<std::string>* deletions) const {
  std::vector<Clipping> ownerRows, modernRows;
  std::vector<std::string> ownerDeleted, modernDeleted;
  const bool ownerOk = readVersionedFile(path, ownerRows, &ownerDeleted, false);
  const bool modernOk = readVersionedFile(path, modernRows, &modernDeleted, true);
  out.clear();
  // Never silently choose between two valid interpretations of the same v4.
  if (ownerOk == modernOk) return false;
  out = ownerOk ? std::move(ownerRows) : std::move(modernRows);
  if (deletions) *deletions = ownerOk ? std::move(ownerDeleted) : std::move(modernDeleted);
  return true;
}

bool ClippingStore::readVersionedFile(const std::string& path, std::vector<Clipping>& out,
                                      std::vector<std::string>* deletions, const bool modernV4) const {
  out.clear();
  HalFile f;
  if (!Storage.openFileForRead("CLIP", path, f)) {
    return false;
  }

  uint8_t version = 0;
  uint16_t count = 0;
  std::string title;
  std::string author;
  std::string storedPath;
  if (!serialization::tryReadPod(f, version) ||
      (version != LEGACY_VERSION && version != TEXT_OFFSET_VERSION && version != LAYOUT_SIGNATURE_VERSION &&
       version != TABLE_VERSION && version != VERSION) ||
      !serialization::tryReadPod(f, count) || !serialization::tryReadString(f, title, CLIPPING_TEXT_MAX) ||
      !serialization::tryReadString(f, author, CLIPPING_TEXT_MAX) ||
      !serialization::tryReadString(f, storedPath, CLIPPING_TEXT_MAX)) {
    f.close();
    LOG_ERR("CLIP", "Failed to read clipping header: %s", path.c_str());
    return false;
  }

  if (count > CLIPPING_MAX_PER_BOOK) {
    LOG_ERR("CLIP", "Clipping count %u exceeds max, file may be corrupt: %s", count, path.c_str());
    f.close();
    return false;
  }

  if (modernV4 && version != TABLE_VERSION) {
    f.close();
    return false;
  }
  std::vector<std::string> tombstones;
  if (version == VERSION) {
    uint16_t n = 0;
    if (!serialization::tryReadPod(f, n) || n > CLIPPING_MAX_PER_BOOK) return false;
    for (uint16_t i = 0; i < n; ++i) {
      char id[65];
      if (f.read(reinterpret_cast<uint8_t*>(id), sizeof(id)) != sizeof(id) || !memchr(id, 0, sizeof(id)) || !id[0])
        return false;
      tombstones.emplace_back(id);
    }
  }
  out.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    Clipping clipping;
    if (!serialization::tryReadPod(f, clipping.spineIndex) || !serialization::tryReadPod(f, clipping.startPage) ||
        !serialization::tryReadPod(f, clipping.endPage) || !serialization::tryReadPod(f, clipping.pageCount) ||
        !serialization::tryReadPod(f, clipping.startWordIndex) ||
        !serialization::tryReadPod(f, clipping.endWordIndex) || !serialization::tryReadPod(f, clipping.wordCount) ||
        !serialization::tryReadPod(f, clipping.paragraphIndex) || !serialization::tryReadPod(f, clipping.timestamp)) {
      f.close();
      LOG_ERR("CLIP", "Clipping file truncated at record %u: %s", i, path.c_str());
      return false;
    }
    if (version >= LAYOUT_SIGNATURE_VERSION && !serialization::tryReadPod(f, clipping.layoutSignature)) {
      f.close();
      LOG_ERR("CLIP", "Clipping file truncated at layout signature, record %u: %s", i, path.c_str());
      return false;
    }
    if (!modernV4 && version >= TABLE_VERSION && !serialization::tryReadPod(f, clipping.tableSelection)) {
      f.close();
      LOG_ERR("CLIP", "Clipping file truncated at table selection, record %u: %s", i, path.c_str());
      return false;
    }
    if (version == VERSION || modernV4) {
      uint8_t pending = 0;
      if (!serialization::tryReadPod(f, clipping.startOffset) || !serialization::tryReadPod(f, clipping.endOffset) ||
          !serialization::tryReadPod(f, clipping.syncRevision) || !serialization::tryReadPod(f, pending) ||
          pending > 1 || f.read(reinterpret_cast<uint8_t*>(clipping.id), sizeof(clipping.id)) != sizeof(clipping.id) ||
          !memchr(clipping.id, 0, sizeof(clipping.id)))
        return false;
      clipping.pendingUpload = pending != 0;
    }
    if (f.read(reinterpret_cast<uint8_t*>(clipping.chapterTitle), sizeof(clipping.chapterTitle)) !=
        sizeof(clipping.chapterTitle)) {
      f.close();
      LOG_ERR("CLIP", "Clipping file truncated at chapter title, record %u: %s", i, path.c_str());
      return false;
    }
    clipping.chapterTitle[sizeof(clipping.chapterTitle) - 1] = '\0';
    if (version == LEGACY_VERSION) {
      uint32_t textLen = 0;
      if (!serialization::tryReadPod(f, textLen)) {
        f.close();
        LOG_ERR("CLIP", "Clipping file truncated at text length, record %u: %s", i, path.c_str());
        return false;
      }
      clipping.textOffset = static_cast<uint32_t>(f.position());
      clipping.textLength = static_cast<uint16_t>(std::min<uint32_t>(textLen, CLIPPING_TEXT_MAX));
      if (textLen > CLIPPING_TEXT_MAX || textLen > f.size() - f.position() || (textLen > 0 && !f.seekCur(textLen))) {
        f.close();
        LOG_ERR("CLIP", "Clipping file truncated at text, record %u: %s", i, path.c_str());
        return false;
      }
    } else {
      if (!serialization::tryReadPod(f, clipping.textLength)) {
        f.close();
        LOG_ERR("CLIP", "Clipping file truncated at text length, record %u: %s", i, path.c_str());
        return false;
      }
      if (clipping.textLength > CLIPPING_TEXT_MAX) {
        f.close();
        LOG_ERR("CLIP", "Clipping text length %u exceeds max, record %u: %s", clipping.textLength, i, path.c_str());
        return false;
      }
      clipping.textOffset = static_cast<uint32_t>(f.position());
      if (clipping.textLength > f.size() - f.position() ||
          (clipping.textLength > 0 && !f.seekCur(clipping.textLength))) {
        f.close();
        LOG_ERR("CLIP", "Clipping file truncated at text, record %u: %s", i, path.c_str());
        return false;
      }
    }
    out.push_back(std::move(clipping));
  }

  const bool complete = f.position() == f.size();
  const bool closed = f.close();
  if (!complete || !closed) {
    out.clear();
    return false;
  }
  if (deletions) *deletions = std::move(tombstones);
  return true;
}

bool ClippingStore::writeToFile(const std::string* replacementText, const size_t replacementIndex,
                                const std::string* sourcePathOverride, const bool preserveOnRenameFailure) {
  if (!sourcePathOverride && Storage.exists(MOVE_PENDING)) return false;
  Storage.mkdir("/.crosspoint");
  Storage.mkdir(CLIPPINGS_DIR);

  const std::string tmpPath = storeFilePath + ".tmp";
  const std::string backupPath = storeFilePath + ".bak";
  (void)preserveOnRenameFailure;  // All new writes obey the stronger conservative contract.
  if (storeFilePath.empty() || clippings.size() > CLIPPING_MAX_PER_BOOK || deletedIds.size() > CLIPPING_MAX_PER_BOOK ||
      Storage.exists(tmpPath.c_str()) || Storage.exists(backupPath.c_str()) ||
      Storage.exists((storeFilePath + ".rename.bak").c_str()) ||
      Storage.exists((storeFilePath + ".migrate.bak").c_str()))
    return false;

  HalFile source;
  const std::string& sourcePath = sourcePathOverride ? *sourcePathOverride : storeFilePath;
  const bool hasSource = Storage.exists(sourcePath.c_str());
  if (hasSource && !Storage.openFileForRead("CLIP", sourcePath, source)) {
    LOG_ERR("CLIP", "Failed to open clipping source for rewrite: %s", sourcePath.c_str());
    return false;
  }

  HalFile f = Storage.open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_EXCL);
  if (!f) {
    if (source) source.close();
    LOG_ERR("CLIP", "Failed to open clipping temp file for write: %s", tmpPath.c_str());
    return false;
  }

  const uint16_t count = static_cast<uint16_t>(std::min<size_t>(clippings.size(), CLIPPING_MAX_PER_BOOK));
  std::vector<uint32_t> newTextOffsets;
  newTextOffsets.reserve(count);
  std::vector<uint16_t> newTextLengths;
  newTextLengths.reserve(count);
  if (!serialization::tryWritePod(f, VERSION) || !serialization::tryWritePod(f, count) ||
      !serialization::tryWriteString(f, bookTitle) || !serialization::tryWriteString(f, bookAuthor) ||
      !serialization::tryWriteString(f, bookFilePath)) {
    LOG_ERR("CLIP", "Failed to write clipping header: %s", tmpPath.c_str());
    f.close();
    if (source) source.close();
    Storage.remove(tmpPath.c_str());
    return false;
  }

  const uint16_t deletionCount = static_cast<uint16_t>(deletedIds.size());
  if (!serialization::tryWritePod(f, deletionCount)) return false;
  for (const auto& id : deletedIds) {
    char bytes[65] = {};
    if (id.empty() || id.size() >= sizeof(bytes)) return false;
    memcpy(bytes, id.data(), id.size());
    if (f.write(reinterpret_cast<const uint8_t*>(bytes), sizeof(bytes)) != sizeof(bytes)) return false;
  }

  for (uint16_t i = 0; i < count; ++i) {
    const Clipping& clipping = clippings[i];
    if (!serialization::tryWritePod(f, clipping.spineIndex) || !serialization::tryWritePod(f, clipping.startPage) ||
        !serialization::tryWritePod(f, clipping.endPage) || !serialization::tryWritePod(f, clipping.pageCount) ||
        !serialization::tryWritePod(f, clipping.startWordIndex) ||
        !serialization::tryWritePod(f, clipping.endWordIndex) || !serialization::tryWritePod(f, clipping.wordCount) ||
        !serialization::tryWritePod(f, clipping.paragraphIndex) || !serialization::tryWritePod(f, clipping.timestamp) ||
        !serialization::tryWritePod(f, clipping.layoutSignature) ||
        !serialization::tryWritePod(f, clipping.tableSelection) ||
        !serialization::tryWritePod(f, clipping.startOffset) || !serialization::tryWritePod(f, clipping.endOffset) ||
        !serialization::tryWritePod(f, clipping.syncRevision) ||
        !serialization::tryWritePod(f, static_cast<uint8_t>(clipping.pendingUpload)) ||
        f.write(reinterpret_cast<const uint8_t*>(clipping.id), sizeof(clipping.id)) != sizeof(clipping.id) ||
        f.write(reinterpret_cast<const uint8_t*>(clipping.chapterTitle), sizeof(clipping.chapterTitle)) !=
            sizeof(clipping.chapterTitle)) {
      LOG_ERR("CLIP", "Failed to write clipping record %u: %s", i, storeFilePath.c_str());
      f.close();
      if (source) source.close();
      Storage.remove(tmpPath.c_str());
      return false;
    }

    const bool useReplacement = replacementText && i == replacementIndex;
    const uint16_t textLen = useReplacement
                                 ? static_cast<uint16_t>(std::min(replacementText->size(), CLIPPING_TEXT_MAX))
                                 : clipping.textLength;
    if (!serialization::tryWritePod(f, textLen)) {
      LOG_ERR("CLIP", "Failed to write clipping text length %u: %s", i, tmpPath.c_str());
      f.close();
      if (source) source.close();
      Storage.remove(tmpPath.c_str());
      return false;
    }

    const uint32_t newTextOffset = static_cast<uint32_t>(f.position());
    bool wroteText = true;
    if (textLen > 0 && useReplacement) {
      wroteText = f.write(reinterpret_cast<const uint8_t*>(replacementText->data()), textLen) == textLen;
    } else if (textLen > 0) {
      wroteText = source && source.seek(clipping.textOffset) && copyBytes(source, f, textLen);
    }
    if (!wroteText) {
      LOG_ERR("CLIP", "Failed to write clipping text %u: %s", i, tmpPath.c_str());
      f.close();
      if (source) source.close();
      Storage.remove(tmpPath.c_str());
      return false;
    }
    newTextOffsets.push_back(newTextOffset);
    newTextLengths.push_back(textLen);
  }

  if (!f.sync()) {
    LOG_ERR("CLIP", "Failed to sync clipping file: %s", tmpPath.c_str());
    f.close();
    if (source) source.close();
    Storage.remove(tmpPath.c_str());
    return false;
  }
  if (!f.close()) {
    if (source) source.close();
    return false;
  }
  if (source && !source.close()) return false;
  const auto verifySnapshot = [&](const std::string& candidate, const std::string& original) {
    std::vector<Clipping> checked;
    std::vector<std::string> checkedDeletions;
    ClippingFileHeader header;
    if (!readClippingFileHeader(candidate, "epub_", header) || header.title != bookTitle ||
        header.author != bookAuthor || header.path != bookFilePath ||
        !readFromFile(candidate, checked, &checkedDeletions) || checked.size() != count ||
        checkedDeletions != deletedIds)
      return false;
    ClippingStore actual;
    actual.storeFilePath = candidate;
    ClippingStore expected;
    expected.storeFilePath = original;
    for (size_t i = 0; i < count; ++i) {
      const auto& a = checked[i];
      const auto& b = clippings[i];
      if (a.spineIndex != b.spineIndex || a.startPage != b.startPage || a.endPage != b.endPage ||
          a.pageCount != b.pageCount || a.startWordIndex != b.startWordIndex || a.endWordIndex != b.endWordIndex ||
          a.wordCount != b.wordCount || a.paragraphIndex != b.paragraphIndex || a.timestamp != b.timestamp ||
          a.layoutSignature != b.layoutSignature || a.tableSelection != b.tableSelection ||
          a.startOffset != b.startOffset || a.endOffset != b.endOffset || a.syncRevision != b.syncRevision ||
          a.pendingUpload != b.pendingUpload || memcmp(a.id, b.id, sizeof(a.id)) ||
          memcmp(a.chapterTitle, b.chapterTitle, sizeof(a.chapterTitle)))
        return false;
      std::string actualText, expectedText;
      if (!actual.readClippingText(a, actualText)) return false;
      if (replacementText && i == replacementIndex)
        expectedText = *replacementText;
      else if (!expected.readClippingText(b, expectedText))
        return false;
      if (actualText != expectedText) return false;
    }
    return true;
  };
  if (!verifySnapshot(tmpPath, sourcePath)) return false;

  const bool replacingDestination = Storage.exists(storeFilePath.c_str());
  if (replacingDestination && !Storage.rename(storeFilePath.c_str(), backupPath.c_str())) {
    LOG_ERR("CLIP", "Failed to back up clipping file: %s", storeFilePath.c_str());
    return false;
  }
  if (!Storage.rename(tmpPath.c_str(), storeFilePath.c_str())) {
    LOG_ERR("CLIP", "Failed to replace clipping file: %s", storeFilePath.c_str());
    return false;  // Negative FAT acknowledgement may already have published: retain all roots.
  }
  const std::string publishedSource =
      sourcePathOverride ? *sourcePathOverride : (replacingDestination ? backupPath : sourcePath);
  if (!verifySnapshot(storeFilePath, publishedSource)) return false;
  if (replacingDestination && !Storage.remove(backupPath.c_str())) return false;
  for (uint16_t i = 0; i < count; ++i) {
    clippings[i].textOffset = newTextOffsets[i];
    clippings[i].textLength = newTextLengths[i];
  }
  return true;
}

bool ClippingStore::hasAnyClippings() {
  std::vector<ClippedBookEntry> books;
  return getAllClippedBooks(books) && !books.empty();
}

bool ClippingStore::getAllClippedBooks(std::vector<ClippedBookEntry>& out) {
  if (!Storage.exists(CLIPPINGS_DIR)) return true;

  const auto files = Storage.listFiles(CLIPPINGS_DIR);
  for (const auto& name : files) {
    const std::string filename = name.c_str();
    if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".bin") continue;
    ClippingFileHeader header;
    const std::string fullPath = std::string(CLIPPINGS_DIR) + "/" + name.c_str();
    if (!readClippingFileHeader(fullPath, name.c_str(), header)) continue;
    if (header.path.empty() || header.count == 0 || !Storage.exists(header.path.c_str())) continue;

    auto existing = std::find_if(out.begin(), out.end(), [&](const ClippedBookEntry& entry) {
      return entry.bookPath == header.path && entry.bookType == header.bookType;
    });
    if (existing != out.end()) {
      existing->count = std::max(existing->count, header.count);
      continue;
    }
    out.push_back({std::move(header.title), std::move(header.author), std::move(header.path),
                   std::move(header.bookType), header.count});
  }
  return true;
}

bool ClippingStore::deleteForFilePath(const std::string& filePath, const std::string& bookType) {
  if (bookType != "epub" || Storage.exists(MOVE_PENDING)) return false;
  const std::string path = existingStorePathForBook(filePath, bookType);
  const std::string canonical = storeFilePathForBook(filePath, bookType);
  const std::string modern =
      std::string(CLIPPINGS_DIR) + "/" + bookType + "_" + std::to_string(std::hash<std::string>{}(filePath)) + ".bin";
  if (canonical != modern && Storage.exists(canonical.c_str()) && Storage.exists(modern.c_str())) return false;
  for (const char* suffix : {".bak", ".tmp", ".rename.bak", ".migrate.bak"})
    if (Storage.exists((path + suffix).c_str())) return false;
  if (Storage.exists(path.c_str()) && !Storage.remove(path.c_str())) return false;
  const std::string journal = path + ".deleted";
  return !Storage.exists(journal.c_str()) || Storage.remove(journal.c_str());
}

bool ClippingStore::migrateForFilePath(const std::string& oldFilePath, const std::string& newFilePath,
                                       const std::string& title, const std::string& author,
                                       const std::string& bookType) {
  RenameMigration migration;
  return beginRenameMigration(oldFilePath, newFilePath, title, author, bookType, migration, true) &&
         commitRenameMigration(migration);
}

bool ClippingStore::beginRenameMigration(const std::string& oldFilePath, const std::string& newFilePath,
                                         const std::string& title, const std::string& author,
                                         const std::string& bookType, RenameMigration& migration,
                                         const bool preserveOnRenameFailure) {
  return beginRenameMigrationImpl(oldFilePath, newFilePath, title, author, bookType, migration, preserveOnRenameFailure,
                                  false);
}

bool ClippingStore::beginRenameMigrationImpl(const std::string& oldFilePath, const std::string& newFilePath,
                                             const std::string& title, const std::string& author,
                                             const std::string& bookType, RenameMigration& migration,
                                             const bool preserveOnRenameFailure, const bool moveOwner) {
  (void)preserveOnRenameFailure;
  migration = {};
  if (!moveOwner && Storage.exists(MOVE_PENDING)) return false;
  if (bookType != "epub") {
    LOG_ERR("CLIP", "Unknown clipping book type for rename migration: %s", bookType.c_str());
    return false;
  }
  if (oldFilePath.empty() || newFilePath.empty() || oldFilePath == newFilePath) return true;

  migration.sourcePath = existingStorePathForBook(oldFilePath, bookType);
  migration.destinationPath = storeFilePathForBook(newFilePath, bookType);
  migration.destinationBackupPath = migration.destinationPath + ".rename.bak";
  // Clippings are not merged. Manual Move refuses every retained destination
  // candidate rather than silently discarding another book's records.
  if (Storage.exists(migration.destinationPath.c_str()) || Storage.exists(migration.destinationBackupPath.c_str()) ||
      Storage.exists((migration.destinationPath + ".bak").c_str()) ||
      Storage.exists((migration.destinationPath + ".tmp").c_str()) ||
      Storage.exists((migration.destinationPath + ".deleted").c_str()) ||
      Storage.exists((migration.destinationPath + ".migrate.bak").c_str()))
    return false;
  if (!Storage.exists(migration.sourcePath.c_str())) return true;
  if (migration.sourcePath == migration.destinationPath) {
    LOG_ERR("CLIP", "Clipping storage hash collision during rename migration");
    return false;
  }

  ClippingStore reader;
  std::vector<Clipping> migratedClippings;
  if (!reader.loadForBookImpl(oldFilePath, title, author, bookType, moveOwner)) {
    LOG_ERR("CLIP", "Failed to load source clippings for rename: %s", migration.sourcePath.c_str());
    return false;
  }

  migration.active = true;
  ClippingStore writer;
  writer.bookFilePath = newFilePath;
  writer.bookTitle = title;
  writer.bookAuthor = author;
  writer.storeFilePath = migration.destinationPath;
  writer.clippings = reader.clippings;
  writer.deletedIds = reader.deletedIds;
  migration.uncertain = true;
  if (!writer.writeToFile(nullptr, SIZE_MAX, &migration.sourcePath, true)) {
    LOG_ERR("CLIP", "Failed to write transactional clipping rename: %s", migration.destinationPath.c_str());
    // Retain candidates; neither rollback nor commit is safe after a negative acknowledgement.
    return false;
  }
  migration.uncertain = false;
  return true;
}

bool ClippingStore::commitRenameMigration(RenameMigration& migration) {
  return commitRenameMigrationImpl(migration, false);
}

bool ClippingStore::commitRenameMigrationImpl(RenameMigration& migration, const bool moveOwner) {
  if (!moveOwner && Storage.exists(MOVE_PENDING)) return false;
  if (migration.uncertain) return false;
  if (!migration.active) return true;
  bool ok = true;
  if (Storage.exists(migration.sourcePath.c_str()) && !Storage.remove(migration.sourcePath.c_str())) {
    LOG_ERR("CLIP", "Failed to delete renamed clipping source: %s", migration.sourcePath.c_str());
    ok = false;
  }
  if (migration.destinationBackedUp && Storage.exists(migration.destinationBackupPath.c_str()) &&
      !Storage.remove(migration.destinationBackupPath.c_str())) {
    LOG_ERR("CLIP", "Failed to delete clipping rename backup: %s", migration.destinationBackupPath.c_str());
    ok = false;
  }
  if (ok) {
    const std::string journal = migration.sourcePath + ".deleted";
    if (Storage.exists(journal.c_str()) && !Storage.remove(journal.c_str())) ok = false;
  }
  if (ok) migration.active = false;
  return ok;
}

bool ClippingStore::rollbackRenameMigration(RenameMigration& migration) {
  if (Storage.exists(MOVE_PENDING)) return false;
  if (migration.uncertain) return false;
  if (!migration.active) return true;
  bool ok = true;
  if (Storage.exists(migration.destinationPath.c_str()) && !Storage.remove(migration.destinationPath.c_str())) {
    LOG_ERR("CLIP", "Failed to remove rolled-back clipping destination: %s", migration.destinationPath.c_str());
    ok = false;
  }
  if (migration.destinationBackedUp &&
      !Storage.rename(migration.destinationBackupPath.c_str(), migration.destinationPath.c_str())) {
    LOG_ERR("CLIP", "Failed to restore clipping rename backup: %s", migration.destinationBackupPath.c_str());
    ok = false;
  }
  if (ok) migration.active = false;
  return ok;
}

namespace {
bool validSyncId(const char (&id)[65]) {
  const char* end = static_cast<const char*>(memchr(id, 0, sizeof(id)));
  if (!end || end == id) return false;
  return std::all_of(id, end, [](unsigned char c) { return c >= 0x20 && c < 0x7f; });
}
}  // namespace

ClippingStore::AddResult ClippingStore::addClipping(uint16_t spine, uint16_t start, uint16_t end, uint16_t pages,
                                                    uint16_t first, uint16_t last, uint16_t words, const char* chapter,
                                                    uint16_t paragraph, const std::string& text, uint32_t layout,
                                                    uint32_t sourceStart, uint32_t sourceEnd) {
  if (!loaded || text.size() > CLIPPING_TEXT_MAX || ((sourceStart == UINT32_MAX) != (sourceEnd == UINT32_MAX)) ||
      (sourceStart != UINT32_MAX && sourceStart >= sourceEnd))
    return AddResult::SaveFailed;
  if (clippings.size() >= CLIPPING_MAX_PER_BOOK) return AddResult::LimitReached;
  Clipping c;
  c.spineIndex = spine;
  c.startPage = start;
  c.endPage = end;
  c.pageCount = std::max<uint16_t>(1, pages);
  c.startWordIndex = first;
  c.endWordIndex = last;
  c.wordCount = words;
  c.paragraphIndex = paragraph;
  c.layoutSignature = layout;
  c.startOffset = sourceStart;
  c.endOffset = sourceEnd;
  c.timestamp = static_cast<uint32_t>(millis() / 1000UL);
  copyBounded(c.chapterTitle, sizeof(c.chapterTitle), chapter);
  clippings.push_back(c);
  if (!writeToFile(&text, clippings.size() - 1)) {
    clippings.pop_back();
    return AddResult::SaveFailed;
  }
  dirty = false;
  return AddResult::Added;
}

bool ClippingStore::prepareSync() {
  if (!loaded) return false;
  const auto previous = clippings;
  for (auto& c : clippings) {
    if (c.id[0]) {
      if (!validSyncId(c.id)) {
        clippings = previous;
        return false;
      }
      continue;
    }
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    static constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(bytes); ++i) {
      c.id[i * 2] = hex[bytes[i] >> 4];
      c.id[i * 2 + 1] = hex[bytes[i] & 15];
    }
    c.id[32] = 0;
    dirty = true;
  }
  if (saveToFile()) return true;
  clippings = previous;
  return false;
}

bool ClippingStore::finishUploads() {
  if (!loaded) return false;
  const auto previous = clippings;
  for (auto& c : clippings)
    if (c.pendingUpload) {
      c.pendingUpload = false;
      dirty = true;
    }
  if (saveToFile()) return true;
  clippings = previous;
  return false;
}

bool ClippingStore::nextDeletion(uint32_t& offset, char (&id)[65]) const {
  id[0] = 0;
  if (!loaded || offset % sizeof(id) || offset / sizeof(id) > deletedIds.size()) return false;
  while (offset / sizeof(id) < deletedIds.size()) {
    const auto& next = deletedIds[offset / sizeof(id)];
    offset += sizeof(id);
    if (std::any_of(clippings.begin(), clippings.end(), [&](const Clipping& c) { return next == c.id; })) continue;
    copyBounded(id, sizeof(id), next.c_str());
    return true;
  }
  return true;
}

bool ClippingStore::finishDeletions() {
  if (!loaded) return false;
  if (deletedIds.empty()) return true;
  const auto previous = deletedIds;
  deletedIds.clear();
  dirty = true;
  if (saveToFile()) {
    const std::string oldJournal = storeFilePath + ".deleted";
    if (Storage.exists(oldJournal.c_str()) && !Storage.remove(oldJournal.c_str())) {
      deletedIds = previous;
      dirty = true;
      return false;
    }
    return true;
  }
  deletedIds = previous;
  return false;
}

bool ClippingStore::applyRemote(const Clipping& remote, const std::string& text, const bool deleted) {
  if (!loaded || !validSyncId(remote.id) ||
      (!deleted && (text.size() > CLIPPING_TEXT_MAX || remote.pageCount == 0 || remote.startPage > remote.endPage ||
                    ((remote.startOffset == UINT32_MAX) != (remote.endOffset == UINT32_MAX)) ||
                    (remote.startOffset != UINT32_MAX && remote.startOffset >= remote.endOffset))))
    return false;
  // A local durable tombstone wins over an older server replay until acknowledged.
  if (!deleted && std::find(deletedIds.begin(), deletedIds.end(), remote.id) != deletedIds.end()) return true;
  auto found =
      std::find_if(clippings.begin(), clippings.end(), [&](const Clipping& c) { return !strcmp(c.id, remote.id); });
  const size_t index = static_cast<size_t>(found - clippings.begin());
  if (found == clippings.end() && deleted) return true;
  if (found != clippings.end() && found->syncRevision > remote.syncRevision) return true;
  if (found != clippings.end() && !deleted && found->syncRevision == remote.syncRevision && !found->pendingUpload)
    return true;
  if (found == clippings.end() && clippings.size() >= CLIPPING_MAX_PER_BOOK) return false;
  const auto previous = clippings;
  if (deleted)
    clippings.erase(found);
  else {
    Clipping c = remote;
    c.pendingUpload = false;
    c.resolvedLayoutBoundaries = 0;
    if (found == clippings.end())
      clippings.push_back(c);
    else
      *found = c;
  }
  if (writeToFile(deleted ? nullptr : &text, index)) {
    dirty = false;
    return true;
  }
  clippings = previous;
  return false;
}

bool ClippingStore::readClippingPreview(size_t index, char* out, size_t size, size_t& length) const {
  length = 0;
  if (!out || size == 0) return false;
  out[0] = 0;
  std::string preview;
  if (!readClippingPreview(index, preview)) return false;
  // Do not silently cut a UTF-8 sequence at the caller's byte bound.
  length = std::min(preview.size(), size - 1);
  while (length < preview.size() && length && (static_cast<unsigned char>(preview[length]) & 0xc0) == 0x80) --length;
  memcpy(out, preview.data(), length);
  out[length] = 0;
  return true;
}

bool ClippingStore::moveBook(const std::string& from, const std::string& to) {
  if (Storage.exists(MOVE_PENDING)) return false;
  if (from == to) return true;
  if (from.empty() || to.empty() || from[0] != '/' || to[0] != '/' || to.compare(0, from.size() + 1, from + "/") == 0 ||
      Storage.exists(to.c_str()))
    return false;
  const auto validBookPath = [](const std::string& path) {
    if (path.size() > CLIPPING_TEXT_MAX || path.back() == '/') return false;
    for (unsigned char c : path)
      if (c < 0x20 || c == 0x7f) return false;
    size_t at = 1;
    while (at < path.size()) {
      const auto end = path.find('/', at);
      const auto component = path.substr(at, end == std::string::npos ? end : end - at);
      if (component.empty() || component == "." || component == "..") return false;
      if (end == std::string::npos) break;
      at = end + 1;
    }
    return true;
  };
  if (!validBookPath(from) || !validBookPath(to)) return false;
  const auto protectedTree = [](const std::string& path) {
    if (path == "/" || protectedpaths::isSensitivePath(path.c_str())) return true;
    size_t end = 1;
    do {
      end = path.find('/', end);
      const std::string prefix = path.substr(0, end);
      if (protectedpaths::isSensitivePath((prefix + "/wifi.json").c_str())) return true;
      if (end != std::string::npos) ++end;
    } while (end != std::string::npos);
    return false;
  };
  if (protectedTree(from) || protectedTree(to)) return false;
  if (instance.loaded &&
      (instance.bookFilePath == from || instance.bookFilePath.compare(0, from.size() + 1, from + "/") == 0)) {
    if (!instance.saveToFile()) return false;
    instance.unload();
  }
  struct Item {
    std::string from, to;
    RenameMigration migration;
  };
  std::vector<Item> items;
  // Enumerate all affected stores, including empty snapshots containing tombstones.
  if (Storage.exists(CLIPPINGS_DIR) && !FsHelpers::directoryCanBeEnumerated(CLIPPINGS_DIR)) return false;
  for (const auto& name : Storage.listFiles(CLIPPINGS_DIR)) {
    const std::string filename = name.c_str();
    if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".bin") continue;
    ClippingFileHeader h;
    const std::string path = std::string(CLIPPINGS_DIR) + "/" + filename;
    if (!readClippingFileHeader(path, filename.c_str(), h)) return false;
    if (h.path != from && h.path.compare(0, from.size() + 1, from + "/") != 0) continue;
    if (items.size() >= CLIPPING_MAX_PER_BOOK) return false;
    items.push_back({h.path, to + h.path.substr(from.size()), {}});
  }
  if (!Storage.exists(from.c_str())) return false;
  Storage.mkdir("/.crosspoint");
  Storage.mkdir(CLIPPINGS_DIR);
  const std::string intent = "CPMOVE1\n" + from + "\n" + to + "\n";
  HalFile marker = Storage.open(MOVE_PENDING, O_WRONLY | O_CREAT | O_EXCL);
  if (!marker || marker.write(reinterpret_cast<const uint8_t*>(intent.data()), intent.size()) != intent.size() ||
      !marker.sync() || !marker.close())
    return false;
  HalFile verify;
  if (!Storage.openFileForRead("CLIP", MOVE_PENDING, verify) || verify.size() != intent.size()) return false;
  std::string readback(intent.size(), '\0');
  if (verify.read(&readback[0], readback.size()) != static_cast<int>(readback.size()) || !verify.close() ||
      readback != intent)
    return false;
  // Prepare conservative copies before touching the book. Never roll back after
  // an unacknowledged FAT operation; retained candidates force explicit recovery.
  for (auto& item : items) {
    ClippingFileHeader h;
    const auto path = existingStorePathForBook(item.from, "epub");
    if (!readClippingFileHeader(path, "epub_", h) ||
        !beginRenameMigrationImpl(item.from, item.to, h.title, h.author, "epub", item.migration, true, true))
      return false;
  }
  if (!Storage.rename(from.c_str(), to.c_str())) return false;
  for (auto& item : items)
    if (!commitRenameMigrationImpl(item.migration, true)) return false;
  return Storage.remove(MOVE_PENDING);
}
