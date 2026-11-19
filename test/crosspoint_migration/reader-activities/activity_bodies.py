#!/usr/bin/env python3
"""Extract unchanged production activity method bodies over narrow state/HAL doubles."""
from pathlib import Path
import re

def method(text, name):
    match = re.search(r'(?:bool|void) '+re.escape(name)+r'\([^;]*?\)\s*(?:const\s*)?\{', text)
    if not match:
        raise RuntimeError('missing production method: '+name)
    pos = match.end(); depth = 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}')
        pos += 1
    return text[match.start():pos]

def emit(root, out):
    code = '''#include <cassert>
#include <cstdio>
#include <memory>
#include <HalStorage.h>
#include <CrossPointSettings.h>
#include "BookStatsTracking.h"
static unsigned long clockMs=1000;
unsigned long millis() { return clockMs; }
struct Book { std::string getCachePath() const { return "/book"; } };
'''
    for cls, book in [('EpubReaderActivity','epub'), ('XtcReaderActivity','xtc')]:
        code += f'''class {cls} {{ public:
  std::unique_ptr<Book> {book}=std::make_unique<Book>();
  BookReadingStats stats;
  GlobalReadingStats globalStats;
  bool statsTrackingActive=true, bookStatsEnabled=true, pendingStatsCommit=false;
  uint32_t sessionReadingSeconds=0;
  uint32_t globalStatsResetRevision=GlobalReadingStats::resetRevision();
  ReadingStatsDateTime sessionStartLocalDateTime;
  bool hasSessionStartLocalDateTime=false;
  unsigned long pageShownAtMs=1000;
  bool activeFootnotePreview=false;
  bool section=true;
  bool currentPageReadingSecondsForStats(uint32_t&, const char*) const;
  void recordCurrentPageReadingTime(const char*);
  void commitReadingStatsSession();
  bool savePendingReadingStats();
  void syncStatsTrackingState();
  void refreshStatsAfterOverlay();
  bool setBookStatsTrackingEnabled(bool);
}};
'''
        text = (root/'src/activities/reader'/f'{cls}.cpp').read_text()
        for name in ['currentPageReadingSecondsForStats','recordCurrentPageReadingTime','commitReadingStatsSession','savePendingReadingStats','syncStatsTrackingState','refreshStatsAfterOverlay','setBookStatsTrackingEnabled']:
            code += method(text,cls+'::'+name)+'\n'
    code += '''template<class Reader> void check() {
 Storage.reset(); SETTINGS.trackReadingStats=true; clockMs=66000;
 Reader r;
 assert(r.setBookStatsTrackingEnabled(false));
 assert(!r.statsTrackingActive && r.sessionReadingSeconds==0);
 assert(r.stats.totalReadingSeconds==65 && r.stats.sessionCount==1);
 assert(BookReadingStats::load("/book").totalReadingSeconds==65);
 clockMs=166000;
 assert(r.setBookStatsTrackingEnabled(true));
 assert(r.statsTrackingActive);
 clockMs=181000;
 r.commitReadingStatsSession();
 assert(r.stats.totalReadingSeconds==80); // not the disabled 100s
 assert(r.savePendingReadingStats());
 assert(r.savePendingReadingStats());
 assert(r.stats.totalReadingSeconds==80 && r.stats.sessionCount==1);
 clockMs=246000; r.pageShownAtMs=181000;
 Storage.failWrite=true;
 assert(!r.setBookStatsTrackingEnabled(false));
 assert(r.statsTrackingActive && r.pendingStatsCommit);
 assert(r.stats.totalReadingSeconds==145);
 Storage.failWrite=false;
 assert(r.setBookStatsTrackingEnabled(false));
 assert(r.stats.totalReadingSeconds==145); // failed-save retry cannot charge twice
 assert(!r.pendingStatsCommit);
 // User edits must not import a stats-screen preview's elapsed counters.
 auto preview=r.stats;preview.totalReadingSeconds+=999;preview.startDateManual=true;preview.startDate={2026,11,18};
 assert(preview.save("/book"));
 r.refreshStatsAfterOverlay();
 assert(r.stats.totalReadingSeconds==145 && r.stats.startDate.day==18);
 // A global reset during an overlay must not be undone by stale reader totals.
 assert(r.setBookStatsTrackingEnabled(true));
 r.sessionReadingSeconds=20;r.pageShownAtMs=0;
 r.globalStats.totalReadingSeconds=1234;
 assert(GlobalReadingStats::resetLocal());
 r.refreshStatsAfterOverlay();
 assert(r.globalStats.totalReadingSeconds==0 && r.sessionReadingSeconds==0);
 assert(r.stats.totalReadingSeconds==165);
 assert(r.savePendingReadingStats());
 assert(GlobalReadingStats::load().totalReadingSeconds==0);
}
int main() {check<EpubReaderActivity>(); check<XtcReaderActivity>();
 puts("PASS production EPUB/XTC disable-flush, elapsed isolation, failed-save retry, no double count");}
'''
    (out/'activities.cpp').write_text(code)
