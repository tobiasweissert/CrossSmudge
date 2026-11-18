#include "CrossPointState.h"

#include <HalStorage.h>
#include <Logging.h>
#include <PersistableStore.h>
#include <Serialization.h>

#include <algorithm>
#include <mutex>

namespace {
constexpr uint8_t STATE_FILE_VERSION = 5;
constexpr char STATE_FILE_BIN[] = "/.crosspoint/state.bin";
constexpr char STATE_FILE_JSON[] = "/.crosspoint/state.json";
constexpr char STATE_FILE_BAK[] = "/.crosspoint/state.bin.bak";
// Private independent copies, not the legacy rename-based .bak/.tmp roots.
constexpr char STATE_JSON_TEMP[] = "/.crosspoint/state.json.checked.tmp";
constexpr char STATE_JSON_BACKUP[] = "/.crosspoint/state.json.checked.bak";
constexpr char STATE_JSON_PENDING[] = "/.crosspoint/state.json.checked.pending";
// Preserve the legacy HAL readFile ceiling for every state-document root.
constexpr size_t STATE_JSON_MAX_BYTES = 50000;

bool readStateJson(const char* path, JsonDocument& doc, std::string* bytes = nullptr) {
  auto file = Storage.open(path);
  if (!file || file.isDirectory()) return false;
  const auto reportedSize = file.size();
  // Check in the storage size type before narrowing or allocating a body buffer.
  if (reportedSize == 0 || reportedSize > STATE_JSON_MAX_BYTES) {
    file.close();
    return false;
  }
  const size_t size = static_cast<size_t>(reportedSize);
  std::string json(size, '\0');
  size_t offset = 0;
  while (offset < size) {
    const size_t count = std::min(size - offset, static_cast<size_t>(256));
    const int read = file.read(&json[offset], count);
    if (read != static_cast<int>(count)) {
      file.close();
      return false;
    }
    offset += count;
  }
  if (!file.close() || json.empty() || deserializeJson(doc, json) || !doc.is<JsonObject>()) return false;
  if (bytes) *bytes = std::move(json);
  return true;
}

bool createStateCopy(const char* path, const std::string& json) {
  // Never truncate a retained candidate, including one left by an uncertain FAT rename.
  auto file = Storage.open(path, O_WRONLY | O_CREAT | O_EXCL);
  if (!file) return false;
  const bool written = file.write(json.data(), json.size()) == json.size();
  const bool synced = file.sync();
  const bool closed = file.close();
  if (!written || !synced || !closed) return false;
  JsonDocument check;
  std::string bytes;
  return readStateJson(path, check, &bytes) && bytes == json;
}

bool writeStateJson(const JsonDocument& doc) {
  // Reject unreadable output before serialization or touching retained candidates.
  const size_t jsonSize = measureJson(doc);
  if (jsonSize == 0 || jsonSize > STATE_JSON_MAX_BYTES) return false;
  // Publication uses independent copies only, never FAT rename (whose false
  // acknowledgement can leave aliased cluster chains). A pending write is
  // read-only across reboot; retain all candidates for external inspection.
  if (Storage.exists(STATE_JSON_PENDING)) return false;
  const bool hadPrimary = Storage.exists(STATE_FILE_JSON);
  JsonDocument oldDoc;
  std::string oldJson;
  if (hadPrimary) {
    // A corrupt/unreadable primary must never rotate over a healthy recovery copy.
    if (!readStateJson(STATE_FILE_JSON, oldDoc, &oldJson)) return false;
  } else if (Storage.exists(STATE_JSON_BACKUP) || Storage.exists(STATE_JSON_TEMP)) {
    return false;
  }
  if (Storage.exists(STATE_JSON_TEMP)) {
    JsonDocument previous;
    std::string bytes;
    // Without a pending marker, only an exact independent copy of the valid
    // primary is a completed write. Never erase an unresolved prepared candidate.
    if (!readStateJson(STATE_JSON_TEMP, previous, &bytes) || bytes != oldJson) return false;
    if (!Storage.remove(STATE_JSON_TEMP)) return false;
  }
  std::string json;
  if (serializeJson(doc, json) != jsonSize) return false;
  Storage.mkdir("/.crosspoint");
  if (!createStateCopy(STATE_JSON_TEMP, json)) return false;
  // The pending root is itself the current old-state snapshot, not just an
  // existence marker: a retained older .bak must not revive an acknowledged cleanup.
  if (!createStateCopy(STATE_JSON_PENDING, hadPrimary ? oldJson : "{}")) return false;
  // Neither sidecar is ever renamed, so replacing this independent backup
  // cannot truncate a primary/candidate alias left by our protocol.
  if (Storage.exists(STATE_JSON_BACKUP) && !Storage.remove(STATE_JSON_BACKUP)) return false;
  if (hadPrimary && !createStateCopy(STATE_JSON_BACKUP, oldJson)) return false;
  // Both the old document and the complete replacement now have independent,
  // checked roots before replacing the primary. An open/write failure cannot
  // discard the only old document.
  if (hadPrimary && !Storage.remove(STATE_FILE_JSON)) return false;
  if (!createStateCopy(STATE_FILE_JSON, json)) return false;
  // Retain .tmp as the latest complete recovery value, including cleared
  // references/tombstones. A stale old backup is never the normal fallback.
  return Storage.remove(STATE_JSON_PENDING);
}
}  // namespace

bool CrossPointState::isRecentSleep(uint16_t idx, uint8_t checkCount) const {
  const uint8_t effectiveCount = std::min(checkCount, recentSleepFill);
  for (uint8_t i = 0; i < effectiveCount; i++) {
    const uint8_t slot = (recentSleepPos + SLEEP_RECENT_COUNT - 1 - i) % SLEEP_RECENT_COUNT;
    if (recentSleepImages[slot] == idx) return true;
  }
  return false;
}

void CrossPointState::pushRecentSleep(uint16_t idx) {
  recentSleepImages[recentSleepPos] = idx;
  recentSleepPos = (recentSleepPos + 1) % SLEEP_RECENT_COUNT;
  if (recentSleepFill < SLEEP_RECENT_COUNT) recentSleepFill++;
}

void CrossPointState::clearRecentSleepHistory() {
  std::fill_n(recentSleepImages, SLEEP_RECENT_COUNT, static_cast<uint16_t>(0));
  recentSleepPos = 0;
  recentSleepFill = 0;
}

void CrossPointState::setPendingOverlayResume(PendingOverlayResume value) {
  pendingOverlayResume = std::move(value);
  saveToFile();
}

bool CrossPointState::consumePendingOverlayResume(PendingOverlayResume& value) {
  if (!consumePendingOverlayResumeOnce(pendingOverlayResume, value)) return false;
  saveToFile();
  return true;
}

void CrossPointState::pushRecentBoot(uint16_t idx) {
  recentBootImages[recentBootPos] = idx;
  recentBootPos = (recentBootPos + 1) % BOOT_RECENT_COUNT;
  if (recentBootFill < BOOT_RECENT_COUNT) recentBootFill++;
}

bool CrossPointState::saveToFile() const {
  std::lock_guard<std::mutex> storeLock(storeMutex);
  std::lock_guard<std::mutex> stateLock(_mutex);
  JsonDocument doc;
  toJson(doc);
  return writeStateJson(doc);
}

bool CrossPointState::loadFromFile() {
  {
    std::lock_guard<std::mutex> storeLock(storeMutex);
    JsonDocument doc;
    // Read only: recovery never renames/removes a possibly aliased candidate.
    // Prefer the primary, then the complete replacement, then the checked
    // current-old snapshot. The archival .bak can precede an acknowledged
    // cleanup and is deliberately never an automatic recovery source.
    if (readStateJson(STATE_FILE_JSON, doc) || readStateJson(STATE_JSON_TEMP, doc) ||
        readStateJson(STATE_JSON_PENDING, doc)) {
      std::lock_guard<std::mutex> stateLock(_mutex);
      return fromJson(doc.as<JsonVariantConst>());
    }
    // Existing JSON/recovery data is not a first boot. Never resurrect an old
    // binary reference after a failed JSON cleanup or unreadable recovery root.
    if (Storage.exists(STATE_FILE_JSON) || Storage.exists(STATE_JSON_BACKUP) || Storage.exists(STATE_JSON_TEMP) ||
        Storage.exists(STATE_JSON_PENDING))
      return false;
  }

  // Fall back to binary migration
  if (Storage.exists(STATE_FILE_BIN)) {
    if (loadFromBinaryFile()) {
      if (saveToFile()) {
        Storage.rename(STATE_FILE_BIN, STATE_FILE_BAK);
        LOG_DBG("CPS", "Migrated state.bin to state.json");
        return true;
      } else {
        LOG_ERR("CPS", "Failed to save state during migration");
        return false;
      }
    }
  }

  return false;
}

void CrossPointState::toJson(JsonDocument& doc) const {
  doc["openEpubPath"] = openEpubPath;
  doc["favoriteSleepImagePath"] = favoriteSleepImagePath;
  doc["preferredSleepFolderPath"] = preferredSleepFolderPath;
  JsonArray recentArr = doc["recentSleepImages"].to<JsonArray>();
  for (int i = 0; i < SLEEP_RECENT_COUNT; i++) recentArr.add(recentSleepImages[i]);
  doc["recentSleepPos"] = recentSleepPos;
  doc["recentSleepFill"] = recentSleepFill;
  doc["favoriteBootImagePath"] = favoriteBootImagePath;
  JsonArray recentBootArr = doc["recentBootImages"].to<JsonArray>();
  for (int i = 0; i < BOOT_RECENT_COUNT; i++) recentBootArr.add(recentBootImages[i]);
  doc["recentBootPos"] = recentBootPos;
  doc["recentBootFill"] = recentBootFill;
  doc["readerActivityLoadCount"] = readerActivityLoadCount;
  doc["lastSleepFromReader"] = lastSleepFromReader;
  doc["pendingBookmarkSpine"] = pendingBookmarkSpine;
  doc["pendingBookmarkProgress"] = pendingBookmarkProgress;
  doc["pendingBookmarkParagraphIndex"] = pendingBookmarkParagraphIndex;
  doc["pendingClippingIndex"] = pendingClippingIndex;
  doc["showBootScreen"] = showBootScreen;
  doc["quickLockResumePending"] = quickLockResumePending;
  doc["quickLockRestoreFrontlight"] = quickLockRestoreFrontlight;
  doc["pendingOverlayOrigin"] = static_cast<uint8_t>(pendingOverlayResume.origin);
  doc["pendingOverlayType"] = static_cast<uint8_t>(pendingOverlayResume.overlay);
  doc["pendingOverlayTab"] = pendingOverlayResume.tab;
  doc["pendingOverlayPane"] = pendingOverlayResume.pane;
  doc["pendingOverlaySelection"] = pendingOverlayResume.selectedIndex;
  doc["pendingOverlayScroll"] = pendingOverlayResume.scrollPosition;
  doc["pendingOverlayBookPath"] = pendingOverlayResume.bookPath;
  doc["pendingOverlayReturnHome"] = pendingOverlayResume.returnHomeAfterReaderFlow;
  doc["pendingOverlayReaderOrientation"] = pendingOverlayResume.readerOrientation;
  doc["pendingOverlayPreserveReaderOrientation"] = pendingOverlayResume.preserveReaderOrientation;
}

bool CrossPointState::fromJson(JsonVariantConst doc) {
  openEpubPath = doc["openEpubPath"] | "";
  favoriteSleepImagePath = doc["favoriteSleepImagePath"] | "";
  preferredSleepFolderPath = doc["preferredSleepFolderPath"] | "";
  std::fill_n(recentSleepImages, SLEEP_RECENT_COUNT, static_cast<uint16_t>(0));
  JsonArrayConst recentArr = doc["recentSleepImages"];
  const int actualCount =
      recentArr.isNull() ? 0 : std::min(static_cast<int>(recentArr.size()), static_cast<int>(SLEEP_RECENT_COUNT));
  for (int i = 0; i < actualCount; i++) recentSleepImages[i] = recentArr[i] | static_cast<uint16_t>(0);
  recentSleepPos = doc["recentSleepPos"] | static_cast<uint8_t>(0);
  if (recentSleepPos >= SLEEP_RECENT_COUNT) {
    recentSleepPos = actualCount > 0 ? recentSleepPos % SLEEP_RECENT_COUNT : 0;
  }
  recentSleepFill = doc["recentSleepFill"] | static_cast<uint8_t>(0);
  recentSleepFill = static_cast<uint8_t>(std::min(static_cast<int>(recentSleepFill), actualCount));
  if (recentSleepFill == 0 && !doc["lastSleepImage"].isNull()) {
    const uint8_t legacy = doc["lastSleepImage"] | static_cast<uint8_t>(UINT8_MAX);
    if (legacy != UINT8_MAX) pushRecentSleep(static_cast<uint16_t>(legacy));
  }
  favoriteBootImagePath = doc["favoriteBootImagePath"] | "";
  std::fill_n(recentBootImages, BOOT_RECENT_COUNT, static_cast<uint16_t>(0));
  JsonArrayConst recentBootArr = doc["recentBootImages"];
  const int actualBootCount =
      recentBootArr.isNull() ? 0
                             : std::min(static_cast<int>(recentBootArr.size()), static_cast<int>(BOOT_RECENT_COUNT));
  for (int i = 0; i < actualBootCount; i++) recentBootImages[i] = recentBootArr[i] | static_cast<uint16_t>(0);
  recentBootPos = doc["recentBootPos"] | static_cast<uint8_t>(0);
  if (recentBootPos >= BOOT_RECENT_COUNT) {
    recentBootPos = actualBootCount > 0 ? recentBootPos % BOOT_RECENT_COUNT : 0;
  }
  recentBootFill = doc["recentBootFill"] | static_cast<uint8_t>(0);
  recentBootFill = static_cast<uint8_t>(std::min(static_cast<int>(recentBootFill), actualBootCount));
  readerActivityLoadCount = doc["readerActivityLoadCount"] | static_cast<uint8_t>(0);
  lastSleepFromReader = doc["lastSleepFromReader"] | false;
  pendingBookmarkSpine = doc["pendingBookmarkSpine"] | static_cast<uint16_t>(UINT16_MAX);
  pendingBookmarkProgress = doc["pendingBookmarkProgress"] | static_cast<float>(-1.0f);
  pendingBookmarkParagraphIndex = doc["pendingBookmarkParagraphIndex"] | static_cast<uint16_t>(UINT16_MAX);
  pendingClippingIndex = doc["pendingClippingIndex"] | static_cast<uint16_t>(UINT16_MAX);
  showBootScreen = doc["showBootScreen"] | true;
  quickLockResumePending = doc["quickLockResumePending"] | false;
  quickLockRestoreFrontlight = doc["quickLockRestoreFrontlight"] | false;
  pendingOverlayResume.origin =
      static_cast<PendingOverlayOrigin>(doc["pendingOverlayOrigin"] | static_cast<uint8_t>(0));
  pendingOverlayResume.overlay = static_cast<PendingOverlayType>(doc["pendingOverlayType"] | static_cast<uint8_t>(0));
  pendingOverlayResume.tab = doc["pendingOverlayTab"] | static_cast<uint8_t>(0);
  pendingOverlayResume.pane = doc["pendingOverlayPane"] | static_cast<uint8_t>(0);
  pendingOverlayResume.selectedIndex = doc["pendingOverlaySelection"] | static_cast<int16_t>(0);
  pendingOverlayResume.scrollPosition = doc["pendingOverlayScroll"] | static_cast<int16_t>(0);
  pendingOverlayResume.bookPath = doc["pendingOverlayBookPath"] | "";
  pendingOverlayResume.returnHomeAfterReaderFlow = doc["pendingOverlayReturnHome"] | false;
  pendingOverlayResume.readerOrientation = doc["pendingOverlayReaderOrientation"] | static_cast<uint8_t>(0);
  pendingOverlayResume.preserveReaderOrientation = doc["pendingOverlayPreserveReaderOrientation"] | false;
  return true;
}

bool CrossPointState::loadFromBinaryFile() {
  HalFile inputFile;
  if (!Storage.openFileForRead("CPS", STATE_FILE_BIN, inputFile)) {
    return false;
  }
  std::lock_guard<std::mutex> lock(_mutex);

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version > STATE_FILE_VERSION) {
    LOG_ERR("CPS", "Deserialization failed: Unknown version %u", version);
    return false;
  }

  serialization::readString(inputFile, openEpubPath);
  if (version >= 2) {
    uint8_t legacyLastSleep = UINT8_MAX;
    serialization::readPod(inputFile, legacyLastSleep);
    if (legacyLastSleep != UINT8_MAX) {
      pushRecentSleep(static_cast<uint16_t>(legacyLastSleep));
    }
  }

  if (version >= 3) {
    serialization::readPod(inputFile, readerActivityLoadCount);
  }

  if (version >= 4) {
    serialization::readPod(inputFile, lastSleepFromReader);
  } else {
    lastSleepFromReader = false;
  }

  if (version >= 5) {
    serialization::readPod(inputFile, pendingBookmarkSpine);
    serialization::readPod(inputFile, pendingBookmarkProgress);
    pendingBookmarkParagraphIndex = UINT16_MAX;
    pendingClippingIndex = UINT16_MAX;
  } else {
    pendingBookmarkSpine = UINT16_MAX;
    pendingBookmarkProgress = -1.0f;
    pendingBookmarkParagraphIndex = UINT16_MAX;
    pendingClippingIndex = UINT16_MAX;
  }

  return true;
}
