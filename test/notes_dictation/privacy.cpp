// Synthetic RTC/log/SD sinks. No real dump handling or external I/O.
#include <DictationPrivacy.h>

#include <cassert>
#include <cstring>
#include <string>
#define IRAM_ATTR
#define LOG_INF(...)
#define LOG_ERR(...)
constexpr int MAX_ENTRY_LEN = 256, MAX_LOG_LINES = 16, LOG_RTC_MAGIC = 1, PANIC_CAPTURE_MAGIC = 2, O_WRITE = 1,
              O_CREAT = 2, O_TRUNC = 4;
volatile uint32_t dictationPrivacy::retained = 0;
char panicMessage[256]{}, logMessages[16][256]{};
size_t logHead = 0;
int rtcLogMagic = 0, panicCaptureMarker = 0;
const char PANIC_REASON_UNKNOWN[] = "unknown";
std::string forwarded;
void __real_panic_abort(const char* reason) { forwarded = reason; }
bool omitPrivateReport = false;
bool isRebootFromPanic() { return true; }
bool sanitizeLogHead() { return false; }
void clearLastLogs() { memset(logMessages, 0, sizeof(logMessages)); }
void clearPanic() {
  clearLastLogs();
  panicMessage[0] = 0;
}
std::string getPanicInfo(bool) { return panicMessage; }
struct File {
  operator bool() { return true; }
  size_t write(const char*, size_t n) { return n; }
  void close() {}
};
struct SD {
  int opens = 0;
  File open(const char*, int) {
    ++opens;
    return {};
  }
} Storage;
#include "privacy.inc"
int main() {
  addToLogRingBuffer("ordinary");
  assert(logHead == 1);
  __wrap_panic_abort("ordinary");
  begin();
  checkPanic();
  assert(Storage.opens == 1);
  dictationPrivacy::arm();
  addToLogRingBuffer("neutral synthetic content");
  assert(logHead == 1);
  __wrap_panic_abort("neutral synthetic reason");
  assert(forwarded == "unknown" && std::string(panicMessage) == "unknown");
  begin();
  checkPanic();
  assert(!dictationPrivacy::active() && !panicMessage[0] && !logMessages[0][0] && Storage.opens == 1);
  puts("PASS real private-boot ring, panic reason and reboot SD report gates; ordinary boot control");
}
