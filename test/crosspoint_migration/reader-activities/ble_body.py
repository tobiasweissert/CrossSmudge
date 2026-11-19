from activity_bodies import method

def emit(root,out):
    (out/'stubs/BlePageTurner.h').write_text('''#pragma once
#include <cstdint>
namespace bleturner {
struct Status {bool running=false,stopping=false,starting=false;};
inline Status radio; inline int requests=0; inline bool finish=false;
enum class BuildRelease {NotHeld,Released};
inline Status status(){return radio;}
inline BuildRelease beforeChapterBuild(){++requests;if(finish)radio={};return finish?BuildRelease::Released:BuildRelease::NotHeld;}
}
''')
    t=(root/'src/activities/reader/EpubReaderActivity.cpp').read_text()
    code='''#include <atomic>
#include <cassert>
#include <cstdio>
#include <Logging.h>
#include "ChapterBuildGate.h"
static uint32_t clockMs=0;
unsigned long millis(){return clockMs;}
void delay(uint32_t step){clockMs+=step;}
constexpr uint32_t BUILD_ROOM_WAIT_MS=3000,BUILD_ROOM_STEP_MS=50;
class EpubReaderActivity {public:
 std::atomic<bool> radioReleasedForBuild{false};bool makeRoomForChapterBuild();
};
'''+method(t,'EpubReaderActivity::makeRoomForChapterBuild')+'''
int main(){
 EpubReaderActivity r;assert(r.makeRoomForChapterBuild());
 bleturner::radio.running=true;bleturner::finish=false;
 clockMs=UINT32_MAX-1000;auto start=clockMs;
 assert(!r.makeRoomForChapterBuild());assert(uint32_t(clockMs-start)==3000);
 bleturner::finish=true;assert(r.makeRoomForChapterBuild());
 assert(r.radioReleasedForBuild && !bleturner::radio.running);
 puts("PASS production EPUB build-room gate: stopped positive, busy refusal, stop completion, monotonic wrap deadline");
}
'''
    (out/'activities.cpp').write_text(code)
