#include "LibraryMetadataCache.h"

#include <Arduino.h>
#include <Logging.h>
#include <Memory.h>
#include <Utf8.h>
#include <ZipFile.h>
#include <expat.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace library {
namespace {
// Source-only, bounded metadata parser: no reader-cache or spine writes.
class SourceMetadataReader final : public Print {
 public:
  SourceMetadataReader(bool container, void (*service)(), bool (*stopped)())
      : container(container), service(service), stopped(stopped) {
    parser = XML_ParserCreate(nullptr);
    if (parser) {
      XML_SetUserData(parser, this);
      XML_SetElementHandler(parser, start, end);
      XML_SetCharacterDataHandler(parser, text);
      XML_SetStartDoctypeDeclHandler(parser, doctype);
      XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_NEVER);
    }
  }
  ~SourceMetadataReader() override {
    if (parser) XML_ParserFree(parser);
  }
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* bytes, size_t size) override {
    if (!parser || done || failed) return 0;
    if (service) service();
    if ((stopped && stopped()) || size > 256u * 1024u - consumed) {
      failed = true;
      return 0;
    }
    consumed += size;
    if (XML_Parse(parser, reinterpret_cast<const char*>(bytes), static_cast<int>(size), false) == XML_STATUS_ERROR &&
        !done)
      failed = true;
    return failed ? 0 : size;
  }
  bool valid() const { return parser && done && !failed; }
  std::string rootfile, title, author, series, genre, seriesIndex;

 private:
  XML_Parser parser = nullptr;
  bool container, done = false, failed = false;
  void (*service)();
  bool (*stopped)();
  size_t consumed = 0;
  unsigned depth = 0, metadataDepth = 0, captureDepth = 0;
  std::string field, value, property, refines, id;
  struct Collection {
    std::string id, title, type, index;
  };
  std::array<Collection, 8> collections;
  size_t collectionCount = 0;
  Collection* collection(const std::string& key) {
    if (key.empty()) return nullptr;
    for (size_t i = 0; i < collectionCount; ++i)
      if (collections[i].id == key) return &collections[i];
    if (collectionCount == collections.size()) return nullptr;
    auto& c = collections[collectionCount++];
    c.id = key;
    return &c;
  }
  static std::string clean(const std::string& value) {
    std::string out;
    bool space = false;
    for (char c : value) {
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        space = !out.empty();
        continue;
      }
      if (space) out.push_back(' ');
      space = false;
      out.push_back(c);
    }
    return out;
  }
  static const char* local(const char* n) {
    const char* c = std::strrchr(n, ':');
    return c ? c + 1 : n;
  }
  static const char* attr(const char** a, const char* key) {
    for (; a && *a; a += 2)
      if (!std::strcmp(*a, key)) return a[1];
    return "";
  }
  static void bounded(std::string& to, const char* from, size_t n) {
    to.append(from, std::min(n, size_t(255) - to.size()));
  }
  void complete() {
    done = true;
    XML_StopParser(parser, XML_FALSE);
  }
  static void doctype(void* u, const char*, const char*, const char*, int) {
    auto& p = *static_cast<SourceMetadataReader*>(u);
    p.failed = true;
    XML_StopParser(p.parser, XML_FALSE);
  }
  static void start(void* u, const char* name, const char** a) {
    auto& p = *static_cast<SourceMetadataReader*>(u);
    ++p.depth;
    const char* tag = local(name);
    if (p.depth > 32) {
      p.failed = true;
      XML_StopParser(p.parser, XML_FALSE);
      return;
    }
    if (p.container) {
      if (!std::strcmp(tag, "rootfile") && p.rootfile.empty() && std::strlen(attr(a, "full-path")) <= 255)
        p.rootfile = attr(a, "full-path");
      return;
    }
    if (!std::strcmp(tag, "metadata") && !p.metadataDepth) p.metadataDepth = p.depth;
    if (!p.metadataDepth || p.depth != p.metadataDepth + 1) return;
    if (std::strlen(tag) > 64) {
      p.failed = true;
      XML_StopParser(p.parser, XML_FALSE);
      return;
    }
    p.field = tag;
    p.value.clear();
    p.captureDepth = p.depth;
    // Validate before copying hostile attribute text into std::string.
    for (const char* key : {"property", "refines", "id"}) {
      if (std::strlen(attr(a, key)) > 255) {
        p.failed = true;
        XML_StopParser(p.parser, XML_FALSE);
        return;
      }
    }
    p.property = attr(a, "property");
    p.refines = attr(a, "refines");
    p.id = attr(a, "id");
    if (p.field == "meta") {
      const char* key = attr(a, "name");
      const char* content = attr(a, "content");
      if (!std::strcmp(key, "calibre:series") && p.series.empty()) bounded(p.series, content, std::strlen(content));
      if (!std::strcmp(key, "calibre:series_index") && p.seriesIndex.empty())
        bounded(p.seriesIndex, content, std::strlen(content));
    }
  }
  static void text(void* u, const char* s, int n) {
    auto& p = *static_cast<SourceMetadataReader*>(u);
    if (p.captureDepth) bounded(p.value, s, static_cast<size_t>(n));
  }
  static void end(void* u, const char* name) {
    auto& p = *static_cast<SourceMetadataReader*>(u);
    if (p.container && !std::strcmp(local(name), "container")) {
      p.complete();
      return;
    }
    if (p.captureDepth == p.depth) {
      p.value = clean(p.value);
      if (p.field == "title" && p.title.empty()) p.title = p.value;
      if (p.field == "creator" && !p.value.empty()) {
        if (!p.author.empty()) bounded(p.author, ", ", 2);
        bounded(p.author, p.value.data(), p.value.size());
      }
      if (p.field == "subject" && p.genre.empty()) p.genre = p.value;
      if (p.field == "meta") {
        if (p.property == "belongs-to-collection") {
          if (auto* c = p.collection(p.id)) c->title = p.value;
        } else if (!p.refines.empty() && p.refines.front() == '#') {
          if (auto* c = p.collection(p.refines.substr(1))) {
            if (p.property == "collection-type") c->type = p.value;
            if (p.property == "group-position") c->index = p.value;
          }
        }
      }
      p.captureDepth = 0;
    }
    if (p.metadataDepth == p.depth && !std::strcmp(local(name), "metadata")) {
      if (p.series.empty())
        for (size_t i = 0; i < p.collectionCount; ++i) {
          const auto& c = p.collections[i];
          if (c.type == "series" && !c.title.empty()) {
            p.series = c.title;
            p.seriesIndex = c.index;
            break;
          }
        }
      p.complete();
      return;
    }
    --p.depth;
  }
};
}  // namespace

bool readLibraryMetadataFromSource(const std::string& path, std::string& title, std::string& author,
                                   std::string& series, std::string& genre, std::string& seriesIndex, void (*service)(),
                                   bool (*stopped)()) {
  ZipFile zip(path);
  if (!zip.open()) return false;
  auto reader = makeUniqueNoThrow<SourceMetadataReader>(true, service, stopped);
  if (!reader) {
    zip.close();
    return false;
  }
  const bool readContainer = zip.readFileToStream("META-INF/container.xml", *reader, 512, true);
  std::string opf = std::move(reader->rootfile);
  const bool safe =
      !opf.empty() && opf.front() != '/' && opf.find("..") == std::string::npos && opf.find('\\') == std::string::npos;
  if (!readContainer || !reader->valid() || !safe) {
    zip.close();
    return false;
  }
  // Release the container parser before allocating the OPF parser. Large
  // parser/string state belongs on the fallible heap, not the input-task stack.
  reader.reset();
  reader = makeUniqueNoThrow<SourceMetadataReader>(false, service, stopped);
  if (!reader) {
    zip.close();
    return false;
  }
  const bool read = zip.readFileToStream(opf.c_str(), *reader, 512, true);
  const bool closed = zip.close();
  if (!read || !closed || !reader->valid()) return false;
  title = utf8ComposeNfc(reader->title);
  author = utf8ComposeNfc(reader->author);
  series = utf8ComposeNfc(reader->series);
  genre = utf8ComposeNfc(reader->genre);
  seriesIndex = reader->seriesIndex;
  return true;
}

namespace {

constexpr char SLOT_PATH[] = "/.crosspoint/crosscub-library-v7.meta";
constexpr char PAYLOAD_PATH[] = "/.crosspoint/crosscub-library-v7.metd";
constexpr char CACHE_MAGIC[4] = {'C', 'L', 'M', '1'};
// Bump when the set of extracted fields changes, so old entries are re-parsed.
constexpr uint8_t CACHE_VERSION = 1;
constexpr uint32_t PAYLOAD_MAGIC = 0x504D4C43u;  // "CLMP"
constexpr uint32_t SLOT_START = 512;
constexpr size_t ZERO_CHUNK_BYTES = 512;
// Bounds what a power cut can cost without syncing after every book.
constexpr uint16_t STORES_PER_SYNC = 16;

static_assert((LibraryMetadataCache::SLOT_COUNT & (LibraryMetadataCache::SLOT_COUNT - 1)) == 0,
              "slot count must be a power of two");

#pragma pack(push, 1)
struct CacheHeader {
  char magic[4];
  uint8_t version;
  uint8_t padding[3];
  uint32_t slotCount;
  uint32_t usedSlots;
  uint8_t reserved[16];
};
static_assert(sizeof(CacheHeader) == 32, "metadata cache header must stay 32 bytes");

struct CacheSlot {
  uint64_t pathHash;
  uint32_t payloadOffset;
  uint32_t check;
};
static_assert(sizeof(CacheSlot) == 16, "metadata cache slots must stay 16 bytes");

struct PayloadHeader {
  uint32_t magic;
  uint64_t pathHash;
  uint32_t fileSize;
  uint32_t modificationTime;
  uint32_t seriesPosition;
  uint8_t titleLen;
  uint8_t authorLen;
  uint8_t seriesLen;
  uint8_t genreLen;
  uint32_t checksum;
};
static_assert(sizeof(PayloadHeader) == 32, "metadata payload header must stay 32 bytes");
#pragma pack(pop)

constexpr uint32_t SLOT_FILE_BYTES = SLOT_START + LibraryMetadataCache::SLOT_COUNT * sizeof(CacheSlot);

uint32_t fnv1a32(const void* data, const size_t len, uint32_t hash = 2166136261u) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; i++) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}

// Never zero, so an all-zero slot is unambiguously empty.
uint32_t slotCheck(const uint64_t pathHash, const uint32_t payloadOffset) {
  return fnv1a32(&payloadOffset, sizeof(payloadOffset), fnv1a32(&pathHash, sizeof(pathHash))) | 1u;
}

uint32_t firstSlotFor(const uint64_t pathHash) {
  return static_cast<uint32_t>(pathHash ^ (pathHash >> 32)) & (LibraryMetadataCache::SLOT_COUNT - 1);
}

uint8_t fieldLength(const std::string& value) {
  const size_t capped = std::min(value.size(), LibraryMetadataCache::MAX_FIELD_BYTES);
  return static_cast<uint8_t>(utf8SafeTruncateBuffer(value.data(), static_cast<int>(capped)));
}

bool readField(HalFile& file, const uint8_t len, std::string& out, uint32_t& checksum) {
  out.resize(len);
  if (len == 0) return true;
  if (file.read(&out[0], len) != static_cast<int>(len)) return false;
  checksum = fnv1a32(out.data(), len, checksum);
  return true;
}

}  // namespace

const char* LibraryMetadataCache::slotPath() { return SLOT_PATH; }
const char* LibraryMetadataCache::payloadPath() { return PAYLOAD_PATH; }

LibraryMetadataCache::~LibraryMetadataCache() { close(); }

void LibraryMetadataCache::open(void (*const serviceFn)(), bool (*const stoppedFn)()) {
  close();
  service = serviceFn;
  stopped = stoppedFn;
  disabled = false;
  if (!Storage.exists(SLOT_PATH)) return;
  if (openExisting()) return;
  // Unreadable, from another version, or worn out by renames. Start over on
  // the next store instead of probing a table that can no longer help.
  LOG_INF("LIBMETA", "resetting metadata cache");
  discard();
}

void LibraryMetadataCache::discard() {
  headerDirty = false;
  close();
  Storage.remove(SLOT_PATH);
  Storage.remove(PAYLOAD_PATH);
}

bool LibraryMetadataCache::openExisting() {
  slots = Storage.open(SLOT_PATH, O_RDWR);
  payload = Storage.open(PAYLOAD_PATH, O_RDWR);
  if (!slots || !payload || slots.fileSize() != SLOT_FILE_BYTES) return false;
  CacheHeader header{};
  if (!slots.seekSet(0) || slots.read(&header, sizeof(header)) != static_cast<int>(sizeof(header))) return false;
  if (memcmp(header.magic, CACHE_MAGIC, sizeof(CACHE_MAGIC)) != 0 || header.version != CACHE_VERSION ||
      header.slotCount != SLOT_COUNT || header.usedSlots > MAX_USED_SLOTS) {
    return false;
  }
  const size_t payloadBytes = payload.fileSize();
  if (payloadBytes > MAX_PAYLOAD_BYTES) return false;
  usedSlots = header.usedSlots;
  payloadEnd = static_cast<uint32_t>(payloadBytes);
  opened = true;
  return true;
}

bool LibraryMetadataCache::create() {
  close();
  Storage.remove(SLOT_PATH);
  Storage.remove(PAYLOAD_PATH);
  // One sector of zeros, reused for the whole table. Internal heap rather than
  // the stack: it is larger than the builder's per-frame budget.
  auto zeros = makeUniqueNoThrow<uint8_t[]>(ZERO_CHUNK_BYTES);
  if (!zeros) {
    LOG_ERR("LIBMETA", "zero buffer alloc failed (%u bytes)", static_cast<unsigned>(ZERO_CHUNK_BYTES));
    return false;
  }
  slots = Storage.open(SLOT_PATH, O_RDWR | O_CREAT | O_TRUNC);
  if (!slots) {
    LOG_ERR("LIBMETA", "cannot create %s", SLOT_PATH);
    return false;
  }
  CacheHeader header{};
  memcpy(header.magic, CACHE_MAGIC, sizeof(CACHE_MAGIC));
  header.version = CACHE_VERSION;
  header.slotCount = SLOT_COUNT;
  memcpy(zeros.get(), &header, sizeof(header));
  for (uint32_t written = 0; written < SLOT_FILE_BYTES; written += ZERO_CHUNK_BYTES) {
    if (slots.write(zeros.get(), ZERO_CHUNK_BYTES) != ZERO_CHUNK_BYTES) {
      // Usually a full card. A partial table would only fail validation and
      // be rewritten next scan, so remove it now.
      LOG_ERR("LIBMETA", "cannot initialise %s at %u", SLOT_PATH, static_cast<unsigned>(written));
      discard();
      return false;
    }
    if (written == 0) memset(zeros.get(), 0, sizeof(header));
    if (service)
      service();
    else if ((written / ZERO_CHUNK_BYTES & 0x3Fu) == 0x3Fu)
      delay(1);
    if (stopped && stopped()) {
      discard();
      return false;
    }
  }
  payload = Storage.open(PAYLOAD_PATH, O_RDWR | O_CREAT | O_TRUNC);
  if (!payload) {
    LOG_ERR("LIBMETA", "cannot create %s", PAYLOAD_PATH);
    discard();
    return false;
  }
  usedSlots = 0;
  payloadEnd = 0;
  opened = true;
  LOG_INF("LIBMETA", "created metadata cache (%u slots)", static_cast<unsigned>(SLOT_COUNT));
  return true;
}

void LibraryMetadataCache::disable() {
  close();
  disabled = true;
}

void LibraryMetadataCache::close() {
  if (opened && headerDirty && slots) {
    CacheHeader header{};
    memcpy(header.magic, CACHE_MAGIC, sizeof(CACHE_MAGIC));
    header.version = CACHE_VERSION;
    header.slotCount = SLOT_COUNT;
    header.usedSlots = usedSlots;
    if (!slots.seekSet(0) || slots.write(&header, sizeof(header)) != sizeof(header))
      LOG_ERR("LIBMETA", "cannot update metadata cache header");
  }
  headerDirty = false;
  if (payload) payload.close();
  if (slots) slots.close();
  opened = false;
  unsyncedStores = 0;
}

bool LibraryMetadataCache::readSlot(const uint32_t index, uint64_t& pathHash, uint32_t& payloadOffset, bool& empty) {
  CacheSlot slot{};
  if (!slots.seekSet(SLOT_START + index * sizeof(CacheSlot)) ||
      slots.read(&slot, sizeof(slot)) != static_cast<int>(sizeof(slot))) {
    return false;
  }
  empty = slot.pathHash == 0 && slot.payloadOffset == 0 && slot.check == 0;
  // A torn slot reads as neither empty nor a match, so probing continues past it.
  pathHash = slot.check == slotCheck(slot.pathHash, slot.payloadOffset) ? slot.pathHash : 0;
  payloadOffset = slot.payloadOffset;
  return true;
}

bool LibraryMetadataCache::writeSlot(const uint32_t index, const uint64_t pathHash, const uint32_t payloadOffset) {
  const CacheSlot slot{pathHash, payloadOffset, slotCheck(pathHash, payloadOffset)};
  return slots.seekSet(SLOT_START + index * sizeof(CacheSlot)) && slots.write(&slot, sizeof(slot)) == sizeof(slot);
}

bool LibraryMetadataCache::lookup(const uint64_t pathHash, const uint32_t fileSize, const uint32_t modificationTime,
                                  CachedBookMetadata& out) {
  if (!opened || pathHash == 0 || modificationTime == 0) return false;
  uint32_t index = firstSlotFor(pathHash);
  for (uint32_t probe = 0; probe < MAX_PROBES; probe++, index = (index + 1) & (SLOT_COUNT - 1)) {
    uint64_t slotHash = 0;
    uint32_t offset = 0;
    bool empty = false;
    if (!readSlot(index, slotHash, offset, empty)) return false;
    if (empty) return false;
    if (slotHash != pathHash) continue;
    // One slot per path: a different size or time is the same book, changed.
    PayloadHeader header{};
    if (offset + sizeof(header) > payloadEnd || !payload.seekSet(offset) ||
        payload.read(&header, sizeof(header)) != static_cast<int>(sizeof(header))) {
      return false;
    }
    if (header.magic != PAYLOAD_MAGIC || header.pathHash != pathHash || header.fileSize != fileSize ||
        header.modificationTime != modificationTime) {
      return false;
    }
    const uint32_t expected = header.checksum;
    header.checksum = 0;
    uint32_t checksum = fnv1a32(&header, sizeof(header));
    if (!readField(payload, header.titleLen, out.title, checksum) ||
        !readField(payload, header.authorLen, out.author, checksum) ||
        !readField(payload, header.seriesLen, out.series, checksum) ||
        !readField(payload, header.genreLen, out.genre, checksum) || checksum != expected) {
      LOG_DBG("LIBMETA", "ignoring unreadable cached metadata at %u", static_cast<unsigned>(offset));
      return false;
    }
    out.seriesPosition = header.seriesPosition;
    return true;
  }
  return false;
}

void LibraryMetadataCache::store(const uint64_t pathHash, const uint32_t fileSize, const uint32_t modificationTime,
                                 const CachedBookMetadata& metadata) {
  if (disabled || pathHash == 0 || modificationTime == 0) return;
  if (!opened && !create()) {
    disable();
    return;
  }

  // Find the slot first so a full neighbourhood skips the payload write too.
  uint32_t index = firstSlotFor(pathHash);
  bool claimsNewSlot = false;
  bool found = false;
  for (uint32_t probe = 0; probe < MAX_PROBES; probe++, index = (index + 1) & (SLOT_COUNT - 1)) {
    uint64_t slotHash = 0;
    uint32_t offset = 0;
    bool empty = false;
    if (!readSlot(index, slotHash, offset, empty)) {
      LOG_ERR("LIBMETA", "metadata cache slot read failed; caching disabled for this scan");
      disable();
      return;
    }
    if (empty || slotHash == 0 || slotHash == pathHash) {
      claimsNewSlot = empty;
      found = true;
      break;
    }
  }
  if (!found) {
    LOG_DBG("LIBMETA", "no free slot near %08x", static_cast<unsigned>(pathHash));
    return;
  }

  PayloadHeader header{};
  header.magic = PAYLOAD_MAGIC;
  header.pathHash = pathHash;
  header.fileSize = fileSize;
  header.modificationTime = modificationTime;
  header.seriesPosition = metadata.seriesPosition;
  header.titleLen = fieldLength(metadata.title);
  header.authorLen = fieldLength(metadata.author);
  header.seriesLen = fieldLength(metadata.series);
  header.genreLen = fieldLength(metadata.genre);
  uint32_t checksum = fnv1a32(&header, sizeof(header));
  checksum = fnv1a32(metadata.title.data(), header.titleLen, checksum);
  checksum = fnv1a32(metadata.author.data(), header.authorLen, checksum);
  checksum = fnv1a32(metadata.series.data(), header.seriesLen, checksum);
  checksum = fnv1a32(metadata.genre.data(), header.genreLen, checksum);
  header.checksum = checksum;

  // Payload before slot: a slot can only ever point at bytes that were
  // written first, and the checksum covers a write the card reordered.
  const uint32_t offset = payloadEnd;
  const size_t bytes = sizeof(header) + header.titleLen + header.authorLen + header.seriesLen + header.genreLen;
  const auto put = [this](const void* data, const size_t len) { return len == 0 || payload.write(data, len) == len; };
  if (!payload.seekSet(offset) || !put(&header, sizeof(header)) || !put(metadata.title.data(), header.titleLen) ||
      !put(metadata.author.data(), header.authorLen) || !put(metadata.series.data(), header.seriesLen) ||
      !put(metadata.genre.data(), header.genreLen)) {
    LOG_ERR("LIBMETA", "metadata cache payload write failed; caching disabled for this scan");
    disable();
    return;
  }
  payloadEnd = offset + static_cast<uint32_t>(bytes);
  if (!writeSlot(index, pathHash, offset)) {
    LOG_ERR("LIBMETA", "metadata cache slot write failed; caching disabled for this scan");
    disable();
    return;
  }
  if (claimsNewSlot) {
    usedSlots++;
    headerDirty = true;
  }
  if (++unsyncedStores >= STORES_PER_SYNC) {
    unsyncedStores = 0;
    payload.sync();
    slots.sync();
  }
}

}  // namespace library
