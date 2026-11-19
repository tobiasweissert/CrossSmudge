#include <CrossPointSettings.h>
#include <HalStorage.h>

#include <cassert>
#include <cstdio>

#include "BookReadingStats.h"
#include "BookStatsTracking.h"
#include "GlobalReadingStats.h"

int main() {
  Storage.reset();
  BookReadingStats b;
  b.totalReadingSeconds = 120;
  b.avgSecondsPerForwardPage = 17;
  b.paceSampleCount = 8;
  b.startDate = {2026, 11, 19};
  assert(b.save("/book"));
  const int writes = Storage.writes;
  assert(b.save("/book"));
  assert(Storage.writes == writes);  // identical save must not wear flash/SD
  auto loaded = BookReadingStats::load("/book");
  assert(loaded.totalReadingSeconds == 120 && loaded.avgSecondsPerForwardPage == 17);
  b.totalReadingSeconds = 130;
  Storage.failWrite = true;
  assert(!b.save("/book"));
  Storage.failWrite = false;
  assert(BookReadingStats::load("/book").totalReadingSeconds == 120);
  assert(b.save("/book"));
  assert(BookStatsTracking::isEnabled("/book"));
  assert(BookStatsTracking::setBookEnabled("/book", false));
  assert(!BookStatsTracking::isEnabled("/book"));
  assert(BookReadingStats::remove("/book"));
  assert(!BookStatsTracking::isBookEnabled("/book"));  // reset doesn't enable tracking
  assert(BookStatsTracking::setBookEnabled("/book", true));
  SETTINGS.trackReadingStats = false;
  assert(!BookStatsTracking::isEnabled("/book"));
  SETTINGS.trackReadingStats = true;
  assert(BookStatsTracking::isEnabled("/book"));
  Storage.failWrite = true;
  assert(!BookStatsTracking::setBookEnabled("/book", false));
  assert(BookStatsTracking::isEnabled("/book"));
  Storage.failWrite = false;
  Storage.failNextRenameFrom("/book/reading_stats_off.tmp");
  assert(!BookStatsTracking::setBookEnabled("/book", false));
  assert(!BookStatsTracking::isBookEnabled("/book"));  // uncertain marker publication stays conservatively off
  assert(Storage.exists("/book/reading_stats_off.tmp"));
  assert(BookStatsTracking::setBookEnabled("/book", true));
  assert(BookStatsTracking::isBookEnabled("/book"));
  b.totalReadingSeconds = 200;
  assert(b.save("/book"));
  b.totalReadingSeconds = 300;
  Storage.failNextRenameFrom("/book/stats_v5.bin.tmp");
  assert(!b.save("/book"));
  assert(BookReadingStats::load("/book").totalReadingSeconds == 200);
  GlobalReadingStats unchanged;
  assert(unchanged.save());
  const int globalWrites = Storage.writes;
  assert(unchanged.save());
  assert(Storage.writes == globalWrites);
  const ReadingStatsDateTime epoch{{2000, 1, 1}, 12, 0, 0};
  unchanged.recordReadingSpan(epoch, 10);
  assert(unchanged.readingHistoryAnchorDay == 0 && (unchanged.readingHistoryBits[0] & 1));
  assert(unchanged.displayLongestReadingStreak() == 1);
  BookReadingStats live;
  GlobalReadingStats global;
  const ReadingStatsDateTime start{{2026, 11, 19}, 23, 59, 30};
  BookStatsTracking::commitSession(live, global, 65, &start);
  assert(live.sessionCount == 1 && global.totalSessions == 1);
  assert(live.totalReadingSeconds == 65 && global.totalReadingSeconds == 65);
  BookReadingStats preview = live;
  preview.totalReadingSeconds += 65;  // stats screen already includes pending time
  preview.startDateManual = true;
  preview.startDate = {2026, 11, 18};
  BookStatsTracking::importEdits(live, preview);
  assert(live.totalReadingSeconds == 65 && live.startDate.day == 18);
  auto rev = GlobalReadingStats::resetRevision();
  assert(GlobalReadingStats::resetLocal());
  assert(GlobalReadingStats::resetRevision() == rev + 1);
  Storage.failWrite = true;
  global.totalSessions = 999;
  assert(!global.save());
  Storage.failWrite = false;
  global.totalSessions = UINT32_MAX;
  live.sessionCount = UINT16_MAX;
  live.totalReadingSeconds = UINT32_MAX - 5;
  BookStatsTracking::commitSession(live, global, 65, nullptr);
  assert(live.sessionCount == UINT16_MAX && global.totalSessions == UINT32_MAX);
  assert(live.totalReadingSeconds == UINT32_MAX);
  HalFile future;
  assert(Storage.openFileForWrite("TEST", "/.crosspoint/global_stats.bin", future));
  uint8_t unknown[GlobalReadingStats::CURRENT_FILE_SIZE] = {};
  unknown[0] = 99;
  future.write(unknown, sizeof(unknown));
  future.close();
  auto protectedStats = GlobalReadingStats::load();
  assert(!protectedStats.save());
  assert(GlobalReadingStats::resetLocal());  // explicit acknowledged reset ends the future-format guard
  protectedStats.totalSessions = 2;
  assert(protectedStats.save());
  puts("PASS stats persistence/unchanged/failure, eligibility, session/date import/saturation/reset revision");
}
