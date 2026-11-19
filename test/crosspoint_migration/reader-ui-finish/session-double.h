#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "ReaderSession.h"
static uint32_t nowMs=0;
uint32_t millis(){return nowMs;}
namespace trustedtime {int64_t trustedNow(){return 1700000000+nowMs/1000;}}
namespace KOReaderDocumentId {std::string calculate(const std::string&){return std::string(32,'a');}}
namespace pluginevents {
 enum class Event {ReaderSession};struct Var {const char* key;const char* value;};
 static bool subscriber=true;static std::vector<uint32_t> durations;
 bool anySubscriber(Event){return subscriber;}
 void emit(Event,const Var* v,int n){for(int i=0;i<n;++i)if(std::string(v[i].key)=="duration_seconds")durations.push_back(static_cast<uint32_t>(std::stoul(v[i].value)));}
}
static int holder=0,locks=0;
int xTaskGetCurrentTaskHandle(){return 1;}
int xSemaphoreGetMutexHolder(int){return holder;}
struct RenderLock {RenderLock(){assert(holder==0);holder=1;++locks;}~RenderLock(){assert(holder==1);holder=0;}};
struct ReaderActivity {
 virtual ~ReaderActivity()=default;
 ReaderSession readerSession;std::atomic<int8_t>pendingSessionTurn{0};std::string bookPath="book";
 bool painted=true,overlay=false;int progress=0;
 int pauses=0,resumes=0;
 void onInputLockChanged(bool locked){if(locked)++pauses;else ++resumes;}
 bool acceptsRemoteInput()const{return painted&&!overlay;}
 int getProgressBasisPoints()const{return progress;}
 void flushReaderSession();
};
struct ActivityManager {
 int renderingMutex=1;bool inputLocked=false;
 std::unique_ptr<ReaderActivity> currentActivity=std::make_unique<ReaderActivity>();
 std::vector<std::unique_ptr<ReaderActivity>> stackActivities;
 void setReaderSessionActive(ReaderActivity&,bool);
 void notifyInputLockChanged(bool);
};
