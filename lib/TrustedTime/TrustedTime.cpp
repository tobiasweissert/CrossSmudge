#include "TrustedTime.h"

#include <Arduino.h>
#include <Logging.h>
#include <Preferences.h>
#include <esp_sntp.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace trustedtime {

namespace {

// Below this the clock was obviously never set (2025-01-01 UTC).
constexpr int64_t MIN_VALID_EPOCH = 1735689600LL;
// Above this a value is a fault, not a date (2100-01-01 UTC). Every supported
// RTC chip stops at 2099, so anything past this is a dead battery or a corrupt read --
// and the floor never lowers again, so adopting one would lock every real clock
// out from then on.
constexpr int64_t MAX_VALID_EPOCH = 4102444800LL;
// How far ahead of the persisted floor an RTC may read. A chip whose calendar is
// a year past what we have already trusted is faulty rather than right, and the
// days either side cover a long power-off: past this the clock stays on the
// floor until SNTP answers.
constexpr int64_t MAX_RTC_LEAD_SECS = 31536000LL;
// NVS wear guard: only rewrite the floor when it moved by at least this much.
constexpr int64_t MIN_ADVANCE_SECS = 60;

constexpr const char* PREFS_NAMESPACE = "cptime";
constexpr const char* PREFS_KEY = "floor";

// Latest time this boot has seen (or restored from NVS). trustedNow() never
// reports earlier, so a backward clock step (a bad SNTP answer, a manual set)
// cannot reopen an expired loan. Updated from the SNTP callback's lwIP task as
// well as the main task; 64-bit atomics are not lock-free on the C3.
int64_t ramFloor = 0;
portMUX_TYPE ramFloorLock = portMUX_INITIALIZER_UNLOCKED;

// Raises the in-RAM floor to `value` if higher; returns the resulting floor.
int64_t raiseFloor(const int64_t value) {
  portENTER_CRITICAL(&ramFloorLock);
  if (value > ramFloor) ramFloor = value;
  const int64_t floor = ramFloor;
  portEXIT_CRITICAL(&ramFloorLock);
  return floor;
}

int64_t readFloor() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, /*readOnly=*/true)) return 0;
  const int64_t value = prefs.getLong64(PREFS_KEY, 0);
  prefs.end();
  return value;
}

void writeFloor(const int64_t value) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, /*readOnly=*/false)) return;
  prefs.putLong64(PREFS_KEY, value);
  prefs.end();
}

// SNTP sync callback (lwIP task context; Preferences/NVS is mutex-guarded).
struct Observation {
  uint32_t sequence;
  SyncSample sample;
};
Observation latest{};
std::mutex configureLock;
unsigned waiters = 0;
Observation snapshot() {
  portENTER_CRITICAL(&ramFloorLock);
  const auto value = latest;
  portEXIT_CRITICAL(&ramFloorLock);
  return value;
}
void onTimeSynced(struct timeval* tv) {
  if (tv && tv->tv_sec >= MIN_VALID_EPOCH && tv->tv_sec <= MAX_VALID_EPOCH) {
    const SyncSample sample{static_cast<uint32_t>(tv->tv_sec), static_cast<uint32_t>(millis())};
    portENTER_CRITICAL(&ramFloorLock);
    latest.sample = sample;
    ++latest.sequence;
    portEXIT_CRITICAL(&ramFloorLock);
  }
  note();
}

void configureSntp() {
  // SNTP uses UTC epochs regardless of the display timezone. Copy TZ before
  // configTzTime replaces the environment entry, as HalClock::syncFromNTP does.
  const char* currentTz = getenv("TZ");
  char timezone[64];
  snprintf(timezone, sizeof(timezone), "%s", currentTz ? currentTz : "UTC0");
  configTzTime(timezone, "pool.ntp.org");
}

}  // namespace

void init() {
  sntp_set_time_sync_notification_cb(&onTimeSynced);
  const int64_t floor = readFloor();
  if (floor < MIN_VALID_EPOCH) return;
  raiseFloor(floor);
  if (static_cast<int64_t>(time(nullptr)) < floor) {
    // Cold boot reset the clock; resume from the floor so time keeps moving
    // forward across power cycles instead of restarting at epoch 0.
    timeval tv = {static_cast<time_t>(floor), 0};
    settimeofday(&tv, nullptr);
    LOG_DBG("TIME", "Clock restored to persisted floor");
  }
}

void note() {
  const int64_t now = static_cast<int64_t>(time(nullptr));
  if (now < MIN_VALID_EPOCH) return;
  raiseFloor(now);
  if (now - readFloor() >= MIN_ADVANCE_SECS) writeFloor(now);
}

void adopt(const int64_t epoch) {
  // Out of range: leave the clock and the floor exactly as they are.
  if (epoch < MIN_VALID_EPOCH || epoch > MAX_VALID_EPOCH) return;
  // Compared against the persisted floor, not the in-RAM one: boot calls this
  // before init(), so the in-RAM floor is still empty and the persisted value is
  // the only record of a time this device has already lived through.
  const int64_t stored = readFloor();
  if (stored >= MIN_VALID_EPOCH && epoch - stored > MAX_RTC_LEAD_SECS) return;
  // Deep sleep keeps the system clock, so a wake arrives with a live clock and
  // an empty floor while the RTC may be the older reading: whichever is newer
  // wins, and only inside the supported window.
  const int64_t now = static_cast<int64_t>(time(nullptr));
  const bool nowPlausible = now >= MIN_VALID_EPOCH && now <= MAX_VALID_EPOCH;
  const int64_t candidate = nowPlausible && now > epoch ? now : epoch;
  if (candidate < raiseFloor(0)) return;  // never rewind below a time we already trust
  raiseFloor(candidate);
  timeval tv = {static_cast<time_t>(candidate), 0};
  settimeofday(&tv, nullptr);
  if (candidate - readFloor() >= MIN_ADVANCE_SECS) writeFloor(candidate);
}

void startSync() {
  std::lock_guard<std::mutex> lock(configureLock);
  if (esp_sntp_enabled()) return;  // running; the sync callback handles the rest
  configureSntp();
}

bool syncNow(const uint32_t timeoutMs) {
  SyncSample sample{};
  return syncCurrent(timeoutMs, sample);
}

bool syncCurrent(uint32_t timeoutMs, SyncSample& sample) {
  sample = {};
  const uint32_t start = millis();
  uint32_t baseline;
  {
    std::lock_guard<std::mutex> lock(configureLock);
    baseline = snapshot().sequence;
    // Join an outstanding request only while it still awaits its notification.
    static uint32_t requestedAt = 0;
    if (!waiters || requestedAt != baseline) {
      requestedAt = baseline;
      configureSntp();
    }
    ++waiters;
  }
  bool ok = false;
  while (static_cast<uint32_t>(millis() - start) <= timeoutMs) {
    const auto current = snapshot();
    if (current.sequence != baseline) {
      sample = current.sample;
      ok = true;
      break;
    }
    delay(25);
  }
  std::lock_guard<std::mutex> lock(configureLock);
  --waiters;
  return ok;
}

int64_t trustedNow() {
  const int64_t now = static_cast<int64_t>(time(nullptr));
  // Never earlier than a time already seen; 0 while neither is trustworthy.
  const int64_t floor = raiseFloor(now >= MIN_VALID_EPOCH ? now : 0);
  return floor >= MIN_VALID_EPOCH ? floor : 0;
}

}  // namespace trustedtime
