// Exact production global power dispatcher, pending replay block, and AirDrop hook.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "radio_power_enum.inc"
#include "util/QuickLockTrigger.h"
namespace {
bool stopped = false;
int originalActions = 0, deferCalls = 0, receiverStops = 0, loops = 0, delays = 0, fallbacks = 0;
QuickLockTrigger replayedTrigger = QuickLockTrigger::None;
std::vector<int> sequence;
}  // namespace
namespace airdrop {
struct Receiver {
  static Receiver& instance() {
    static Receiver r;
    return r;
  }
  bool stopped() const { return ::stopped; }
};
}  // namespace airdrop
struct AirDropReceiveActivity {
  bool powerDeferred_ = false;
  void beginLeaving(bool) { ++receiverStops; }
  bool deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN);
};
#include "radio_power_activity.inc"
static void original() {
  assert(stopped);
  ++originalActions;
  sequence.push_back(3);
}
struct Manager {
  AirDropReceiveActivity activity;
  bool deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN action) {
    ++deferCalls;
    sequence.push_back(1);
    return activity.deferGlobalPowerAction(action);
  }
  void loop() {
    ++loops;
    sequence.push_back(2);
  }
  bool requestManualReaderRefresh() { return false; }
  bool canSnapshotForSleepOverlay() { return false; }
  void goToFileTransfer() { original(); }
  void goToCalibreWireless() { original(); }
  void goToJoinNetworkFileTransfer() { original(); }
  void goToHotspotFileTransfer() { original(); }
  void persistGlobalSettings() {}
  void requestUpdate() {}
  bool handleShortcutAction(CrossPointSettings::SHORT_PWRBTN) {
    ++fallbacks;
    return true;
  }
} activityManager;
struct {
  void toggleQuickLock(uint32_t, QuickLockTrigger trigger, bool) {
    original();
    replayedTrigger = trigger;
  }
} buttonShortcutController;
struct FakeRenderer {
  void displayBuffer(int) { original(); }
} renderer;
struct RenderLock {};
struct {
  bool present() { return true; }
  bool isOn() { return false; }
  void setOn(bool) { original(); }
} Frontlight;
struct {
  bool hasTouch() { return true; }
} gpio;
struct {
  uint8_t frontlightOn = 0, disableReaderTouchscreen = 0;
} settings;
#define SETTINGS settings
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
enum { STR_TOUCHSCREEN_DISABLED, STR_TOUCHSCREEN_ENABLED };
const char* tr(int) { return "touch"; }
namespace ScreenshotUtil {
void takeScreenshot(FakeRenderer&) { original(); }
}  // namespace ScreenshotUtil
namespace BookActions {
void drawToast(FakeRenderer&, const char*) { original(); }
}  // namespace BookActions
static uint32_t millis() { return 123; }
static void delay(int) {
  ++delays;
  sequence.push_back(4);
}
static void enterDeepSleep() { original(); }
static int manualScreenRefreshMode() { return 0; }
static bool startGlobalSyncProgress() {
  original();
  return true;
}
static void notifyQuickLockChanged() {}
bool handleGlobalPowerButtonAction(CrossPointSettings::SHORT_PWRBTN, QuickLockTrigger = QuickLockTrigger::None);
#include "radio_power_dispatch.inc"
static void replayTick() {
#include "radio_power_replay.inc"
}
int main() {
  // Idle IGNORE and reader-only actions must never stop a receiver or enter pending.
  assert(!handleGlobalPowerButtonAction(CrossPointSettings::SHORT_PWRBTN::IGNORE));
  assert(deferCalls == 0 && receiverStops == 0 && !exclusivePowerActionPending && originalActions == 0);
  for (auto action : {CrossPointSettings::SHORT_PWRBTN::PAGE_TURN, CrossPointSettings::SHORT_PWRBTN::PREVIOUS_PAGE,
                      CrossPointSettings::SHORT_PWRBTN::TOGGLE_FONT, CrossPointSettings::SHORT_PWRBTN::LOOKUP_WORD}) {
    const bool consumed = handleGlobalPowerButtonAction(action);
    assert(dispatchShortcutAction(action));
    assert(!consumed && !exclusivePowerActionPending && receiverStops == 0 && deferCalls == 0);
  }
  assert(fallbacks == 4);
  // Invalid Quick Lock calls retain their previous rejection without revoking radio ownership.
  assert(!handleGlobalPowerButtonAction(CrossPointSettings::SHORT_PWRBTN::QUICK_LOCK, QuickLockTrigger::None));
  assert(deferCalls == 0 && receiverStops == 0 && !exclusivePowerActionPending && originalActions == 0);
  // Each real global action queues, performs zero original action until stopped,
  // services current activity while waiting, then replays once with trigger preserved.
  for (auto action :
       {CrossPointSettings::SHORT_PWRBTN::SLEEP, CrossPointSettings::SHORT_PWRBTN::QUICK_LOCK,
        CrossPointSettings::SHORT_PWRBTN::FORCE_REFRESH, CrossPointSettings::SHORT_PWRBTN::SCREENSHOT,
        CrossPointSettings::SHORT_PWRBTN::SYNC_PROGRESS, CrossPointSettings::SHORT_PWRBTN::FILE_TRANSFER,
        CrossPointSettings::SHORT_PWRBTN::CALIBRE_WIRELESS, CrossPointSettings::SHORT_PWRBTN::JOIN_NETWORK,
        CrossPointSettings::SHORT_PWRBTN::CREATE_HOTSPOT, CrossPointSettings::SHORT_PWRBTN::TOGGLE_FRONTLIGHT,
        CrossPointSettings::SHORT_PWRBTN::TOGGLE_TOUCHSCREEN}) {
    stopped = false;
    int actionsBefore = originalActions, loopsBefore = loops;
    assert(handleGlobalPowerButtonAction(action, QuickLockTrigger::LongPower));
    assert(exclusivePowerActionPending && exclusivePowerAction == action &&
           exclusivePowerTrigger == QuickLockTrigger::LongPower);
    assert(originalActions == actionsBefore);
    for (int i = 0; i < 3; ++i) {
      sequence.clear();
      replayTick();
      assert(exclusivePowerActionPending && originalActions == actionsBefore);
      assert(sequence.size() == 3 && sequence[0] == 1 && sequence[1] == 2 && sequence[2] == 4);
    }
    assert(loops == loopsBefore + 3);
    stopped = true;
    replayTick();
    assert(!exclusivePowerActionPending && originalActions == actionsBefore + 1);
    replayTick();
    assert(originalActions == actionsBefore + 1);
    if (action == CrossPointSettings::SHORT_PWRBTN::QUICK_LOCK) assert(replayedTrigger == QuickLockTrigger::LongPower);
  }
  std::puts(
      "PASS exact main power dispatcher/replay + AirDrop hook: idle/reader fallback untouched; no action before "
      "stopped; ordered waits; one replay/trigger");
}
