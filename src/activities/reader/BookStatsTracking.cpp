#include "BookStatsTracking.h"

#include <HalStorage.h>
#include <Logging.h>

#include "CrossPointSettings.h"

namespace {
constexpr char MARKER_NAME[] = "/reading_stats_off";

std::string markerPath(const std::string& cachePath) { return cachePath + MARKER_NAME; }
}  // namespace

namespace BookStatsTracking {
void commitSession(BookReadingStats& book, GlobalReadingStats& global, const uint32_t seconds,
                   const ReadingStatsDateTime* start) {
  if (seconds >= 60) {
    if (book.sessionCount < UINT16_MAX) ++book.sessionCount;
    if (global.totalSessions < UINT32_MAX) ++global.totalSessions;
  }
  if (seconds < 10) return;
  book.totalReadingSeconds =
      seconds > UINT32_MAX - book.totalReadingSeconds ? UINT32_MAX : book.totalReadingSeconds + seconds;
  global.totalReadingSeconds =
      seconds > UINT32_MAX - global.totalReadingSeconds ? UINT32_MAX : global.totalReadingSeconds + seconds;
  if (start && start->isValid()) {
    book.recordReadingSpan(*start, seconds);
    global.recordReadingSpan(*start, seconds);
    if (seconds >= 120 && !book.startDateManual && !book.startDate.isValid()) book.startDate = start->date;
  }
}
void importEdits(BookReadingStats& live, const BookReadingStats& disk) {
  live.isCompleted = disk.isCompleted;
  live.startDateManual = disk.startDateManual;
  live.finishedDateManual = disk.finishedDateManual;
  live.startDate = disk.startDate;
  live.finishedDate = disk.finishedDate;
}
bool isBookEnabled(const std::string& cachePath) {
  return cachePath.empty() ||
         (!Storage.exists(markerPath(cachePath).c_str()) && !Storage.exists((markerPath(cachePath) + ".tmp").c_str()));
}

bool isEnabled(const std::string& cachePath) { return SETTINGS.shouldTrackReadingStats() && isBookEnabled(cachePath); }

bool setBookEnabled(const std::string& cachePath, const bool enabled) {
  if (cachePath.empty()) {
    LOG_ERR("BSTAT", "Missing cache path for book tracking setting");
    return false;
  }
  const std::string path = markerPath(cachePath);
  if (enabled) {
    const std::string pending = path + ".tmp";
    if (Storage.exists(pending.c_str()) && !Storage.remove(pending.c_str())) return false;
    if (!Storage.exists(path.c_str())) return true;
    if (Storage.remove(path.c_str())) return true;
    LOG_ERR("BSTAT", "Could not enable book stats: %s", path.c_str());
    return false;
  }
  if (Storage.exists(path.c_str())) return true;
  if (Storage.exists((path + ".tmp").c_str())) return false;
  if (!Storage.exists(cachePath.c_str()) && !Storage.mkdir(cachePath.c_str())) {
    LOG_ERR("BSTAT", "Could not create book cache: %s", cachePath.c_str());
    return false;
  }
  const std::string tempPath = path + ".tmp";
  HalFile file;
  if (!Storage.openFileForWrite("BSTAT", tempPath, file)) {
    LOG_ERR("BSTAT", "Could not disable book stats: %s", path.c_str());
    return false;
  }
  const uint8_t marker = 1;
  const bool written = file.write(&marker, sizeof(marker)) == sizeof(marker) && file.sync();
  const bool closed = file.close();
  if (!written || !closed || !Storage.rename(tempPath.c_str(), path.c_str())) {
    LOG_ERR("BSTAT", "Could not finish book tracking setting: %s", path.c_str());
    return false;
  }
  return true;
}
}  // namespace BookStatsTracking
