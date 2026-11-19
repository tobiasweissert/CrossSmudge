#pragma once

#include <string>
#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

// A book is enabled by default. The marker is separate from stats_v5.bin so
// clearing statistics or EPUB reader settings does not reset the user's choice.
namespace BookStatsTracking {
// Consume elapsed time once in the caller; persistence retries never add it again.
void commitSession(BookReadingStats& book, GlobalReadingStats& global, uint32_t seconds,
                   const ReadingStatsDateTime* start);
// Import only user-editable fields, not preview totals which include pending time.
void importEdits(BookReadingStats& live, const BookReadingStats& disk);
bool isBookEnabled(const std::string& cachePath);
bool isEnabled(const std::string& cachePath);
bool setBookEnabled(const std::string& cachePath, bool enabled);
}  // namespace BookStatsTracking
