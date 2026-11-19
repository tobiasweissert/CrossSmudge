// Actual coordinator methods/callbacks and actual LuaAppActivityAdapter; only VM,
// storage resolver, receiver, allocation and device boundaries are explicit seams.
#include <GfxRenderer.h>
#include <HalLuaHttp.h>
#include <MappedInputManager.h>
#include <Memory.h>
#include <browser/BrowserLaunch.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "activities/Activity.h"
#include "components/AirDropConsentInput.h"
#include "network/AirDropReceiver.h"
struct HostRunner {
  inline static HostRunner* live = nullptr;
  inline static bool initOk = true, loadOk = true, cooperative = true;
  inline static int loads = 0;
  bool error = false;
  std::string message;
  airdrop::browser::LaunchHandle launch;
  uint32_t attempt = 0;
  HostRunner(GfxRenderer&, MappedInputManager&, const std::string&, const std::string&) { live = this; }
  ~HostRunner() {
    if (live == this) live = nullptr;
  }
  bool init() {
    if (!initOk) {
      error = true;
      message = "init failed";
    }
    return initOk;
  }
  bool setBrowserLaunch(const airdrop::browser::LaunchHandle& l, uint32_t a) {
    launch = l;
    attempt = a;
    return true;
  }
  bool loadScript(const std::string&) {
    ++loads;
    if (!loadOk) {
      error = true;
      message = "load failed";
    }
    return loadOk;
  }
  bool networkEnabled() { return HalLuaHttp::networkEnabled(); }
  bool cooperativeBrowser() { return cooperative; }
  bool preventAutoSleep() { return false; }
  bool shouldExit() { return false; }
  bool hasError() { return error; }
  const std::string& getErrorMessage() { return message; }
  void requestNativeCancel() {}
  void clearNativeCancel() {}
  void onInit() {}
  void onUpdate() {}
  void onDraw() {}
  void onButton(const char*, bool) {}
  void onTap(int, int) {}
  void onTouch(int, int, const char*) {}
  bool hasLongPressHandler() { return false; }
  void onLongPress(int, int) {}
  bool hasActiveTouchHold() { return false; }
  void onTouchRelease() {}
  void onSwipe(const char*) {}
  bool checkAndClearRedrawRequested() { return false; }
  bool checkAndClearFullRefreshRequested() { return false; }
  void onExit() {}
  void shutdown() { launch = {}; }
};
#define LuaAppActivity UnusedDefaultLuaAppActivity
#include "LuaAppActivity.h"
#undef LuaAppActivity
using LuaAppActivity = LuaAppActivityAdapter<HostRunner>;
using namespace airdrop::browser;
static bool receiverStopped = true, nativePending = true;
static airdrop::Status snapshotStatus;
static int resolutions = 0, cancellations = 0, resolveAttempts = 0, receiverStops = 0, reaps = 0, resolverCalls = 0;
namespace airdrop {
Receiver& Receiver::instance() {
  static Receiver receiver;
  return receiver;
}
bool Receiver::stopped() const { return receiverStopped; }
bool Receiver::begin() { return false; }
void Receiver::snapshot(Status& out) const { out = snapshotStatus; }
bool Receiver::decide(const OfferIdentity&, bool) { return false; }
void Receiver::requestStop() { ++receiverStops; }
void Receiver::reap() {
  assert(receiverStopped);
  ++reaps;
}
bool Receiver::resolvePendingUrl(const OfferIdentity&, bool opened) {
  ++resolveAttempts;
  if (!receiverStopped || !nativePending) return false;
  nativePending = false;
  if (opened)
    ++resolutions;
  else
    ++cancellations;
  return true;
}
}  // namespace airdrop
struct AppPackage {
  enum class BrowserResolveError { None, Missing, InvalidManifest, Incompatible, InvalidEntry, Storage, Allocation };
  inline static BrowserResolveError result = BrowserResolveError::None;
  std::string path = "/apps/browser", name = "Browser", entryScript = "main.lua";
  static bool resolveInstalledBrowser(AppPackage&, BrowserResolveError& error) {
    ++resolverCalls;
    error = result;
    return result == BrowserResolveError::None;
  }
};
namespace fui {
struct InputSnapshot {
  bool touchPressed = false, touchReleased = false;
};
struct ActionEvent {
  explicit operator bool() const { return false; }
};
}  // namespace fui
namespace TouchHeaderBackButton {
bool wasTapped(const MappedInputManager&, const GfxRenderer&) { return false; }
}  // namespace TouchHeaderBackButton
static fui::InputSnapshot touchSnapshotFrom(const MappedInputManager&) { return {}; }
struct FakeUi {
  bool closed = false;
  void closeRouting() { closed = true; }
  bool routeTouch(MappedInputManager&, fui::ActionEvent&) { return false; }
};
struct Manager {
  int exits = 0;
  void returnFromAirDrop(const std::string&, bool) {
    assert(receiverStopped);
    ++exits;
  }
} activityManager;
constexpr uint64_t PaintIntervalMs = 1200;
struct AirDropReceiveActivity : Activity {
  airdrop::Status status_{};
  LaunchHandle launch_;
  AppPackage browserPackage_;
  FakeUi ui_;
  bool paintedPending_ = false, leaving_ = false, homeRequested_ = false, powerDeferred_ = false, childPending_ = false;
  bool browserAttempted_ = false, browserFailed_ = false, wifiOwned_ = false, handoffResolved_ = false;
  bool radioStopped_ = false, startFailed_ = false, decisionSent_ = false;
  uint64_t lastPaintRequestMs_ = 0;
  airdrop::OfferIdentity paintedIdentity_{};
  std::string returnBookPath_;
  AirDropReceiveActivity(GfxRenderer& r, MappedInputManager& i) : Activity("AirDrop", r, i) {
    status_.state = airdrop::State::UrlReady;
    status_.offer.kind = airdrop::OfferKind::Url;
    std::strcpy(status_.offer.url, "https://example.com/a#fragment");
    status_.identity.offer = 1;
  }
  void resolveHandoff(bool);
  void onExit() override;
  void beginLeaving(bool);
  bool handleHomeGesture() override;
  void setBrowserFailed();
  void startBrowserHandoff();
  void openBrowserChild();
  static bool sameIdentity(const airdrop::OfferIdentity&, const airdrop::OfferIdentity&);
  airdrop::input::ConsentGate consentInput_;
  uint32_t routedEpoch_ = 0;
  airdrop::OfferIdentity routedIdentity_{};
  bool routeConsentInput();
  void decide(bool);
  void retryReceiver();
  void loop() override;
};
#include "coordinator_extracted.inc"
static void reset() {
  receiverStopped = true;
  nativePending = true;
  resolutions = cancellations = resolveAttempts = receiverStops = reaps = resolverCalls = 0;
  HalLuaHttp::connected = false;
  HalLuaHttp::enabled = true;
  HalLuaHttp::disconnects = HalLuaHttp::enabledReads = 0;
  WifiSelectionActivity::starts = WifiSelectionActivity::exits = 0;
  HostRunner::initOk = HostRunner::loadOk = HostRunner::cooperative = true;
  HostRunner::loads = 0;
  AppPackage::result = AppPackage::BrowserResolveError::None;
  coordinator::failAllocation<LaunchRequest> = false;
  coordinator::failAllocation<WifiSelectionActivity> = false;
  coordinator::failAllocation<LuaAppActivity> = false;
  coordinator::failAllocation<HostRunner> = false;
}
// Match ActivityManager ordering: child onExit/destruction precedes parent's moved result callback.
static void pop(AirDropReceiveActivity& parent, bool cancel = false) {
  assert(parent.child && parent.resultCallback);
  auto child = std::move(parent.child);
  auto callback = std::move(parent.resultCallback);
  child->onExit();
  child.reset();
  callback(ActivityResult{cancel});
}
static void wifi(AirDropReceiveActivity& parent, bool connected, bool cancel = false) {
  auto* selector = dynamic_cast<WifiSelectionActivity*>(parent.child.get());
  assert(selector);
  selector->onEnter();
  selector->resultConnected = connected;
  HalLuaHttp::connected = connected;
  pop(parent, cancel);
}
static void idle(AirDropReceiveActivity& parent) {
  snapshotStatus = parent.status_;
  snapshotStatus.nowMs += 10;
  parent.loop();
}
static LuaAppActivity& browser(AirDropReceiveActivity& parent) {
  auto* p = dynamic_cast<LuaAppActivity*>(parent.child.get());
  assert(p);
  p->onEnter();
  return *p;
}
int main() {
  GfxRenderer renderer;
  MappedInputManager input;
  // No radio stop means no resolver, network, allocation or child launch.
  {
    reset();
    receiverStopped = false;
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(!a.child && !a.launch_ && resolverCalls == 0 && nativePending);
  }
  // Files and invalid absolute URL schemes cannot become Browser launch payloads.
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.status_.state = airdrop::State::Success;
    a.status_.offer.kind = airdrop::OfferKind::File;
    a.startBrowserHandoff();
    idle(a);
    assert(!a.child && !a.launch_ && resolverCalls == 0 && resolveAttempts == 0);
  }
  for (const char* invalid : {"http://example.com/", "javascript:alert(1)"}) {
    reset();
    AirDropReceiveActivity a(renderer, input);
    std::strcpy(a.status_.offer.url, invalid);
    a.startBrowserHandoff();
    assert(a.browserFailed_ && !a.child && !a.launch_ && !HalLuaHttp::connected && nativePending &&
           resolveAttempts == 0);
  }
  // HTTPS optout: no selector/resolver/child; native payload remains retryable.
  {
    reset();
    HalLuaHttp::enabled = false;
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(a.browserFailed_ && !a.child && !a.launch_ && resolverCalls == 0 && nativePending && resolveAttempts == 0);
  }
  // All package-resolution failures stop before restoring network or taking URL.
  for (auto error : {AppPackage::BrowserResolveError::Missing, AppPackage::BrowserResolveError::InvalidManifest,
                     AppPackage::BrowserResolveError::Incompatible, AppPackage::BrowserResolveError::Storage,
                     AppPackage::BrowserResolveError::InvalidEntry, AppPackage::BrowserResolveError::Allocation}) {
    reset();
    AppPackage::result = error;
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(a.browserFailed_ && !a.child && !a.launch_ && WifiSelectionActivity::starts == 0 && nativePending &&
           resolveAttempts == 0);
  }
  // Selector cancellation/failure retains the exact launch object, with explicit retry only.
  for (bool cancelled : {false, true}) {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(a.child && a.childPending_ && a.wifiOwned_);
    auto retained = a.launch_;
    wifi(a, false, cancelled);
    assert(a.browserFailed_ && !a.child && retained->state() == LaunchState::Pending && nativePending);
    a.startBrowserHandoff();
    assert(a.child && a.launch_.operator->() == retained.operator->());
    wifi(a, true);
    assert(dynamic_cast<LuaAppActivity*>(a.child.get()) && !a.wifiOwned_);
    auto& child = browser(a);
    assert(!child.finished);
    pop(a);
    assert(a.browserFailed_ && retained->state() == LaunchState::Pending && nativePending && !HalLuaHttp::connected);
  }
  // Every allocation failure retains URL. Child allocation cannot steal STA ownership.
  {
    reset();
    coordinator::failAllocation<LaunchRequest> = true;
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(a.browserFailed_ && !a.child && !a.launch_ && nativePending);
  }
  {
    reset();
    coordinator::failAllocation<WifiSelectionActivity> = true;
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(a.browserFailed_ && !a.child && a.launch_ && !a.wifiOwned_ && nativePending);
  }
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    coordinator::failAllocation<LuaAppActivity> = true;
    wifi(a, true);
    assert(a.browserFailed_ && !a.child && a.wifiOwned_ && HalLuaHttp::connected && nativePending);
    a.onExit();
    assert(!HalLuaHttp::connected && cancellations == 1 && HalLuaHttp::disconnects == 1);
  }
  // Selector cancel/failure retains the same request through an explicit successful retry.
  for (bool cancelled : {false, true}) {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    auto* request = a.launch_.operator->();
    wifi(a, false, cancelled);
    assert(a.browserFailed_ && !a.child && nativePending && resolutions == 0);
    assert(a.launch_->state() == LaunchState::Pending);
    const int before = resolverCalls;
    for (int i = 0; i < 5; ++i) idle(a);
    assert(!a.child && resolverCalls == before && nativePending);
    a.startBrowserHandoff();
    assert(a.child && a.launch_.operator->() == request);
    wifi(a, true);
    browser(a);
    auto attempt = HostRunner::live->attempt;
    assert(a.launch_->take(attempt));
    assert(a.launch_->acknowledge(attempt));
    pop(a);
    assert(resolutions == 1 && cancellations == 0 && !nativePending);
    a.onExit();
    assert(resolutions == 1);
  }
  // A package changed/removed during WiFi selection is revalidated before child allocation.
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    AppPackage::result = AppPackage::BrowserResolveError::Incompatible;
    wifi(a, true);
    assert(a.browserFailed_ && !a.child && a.wifiOwned_ && resolverCalls == 2 && nativePending);
    a.onExit();
    assert(!HalLuaHttp::connected && cancellations == 1);
  }
  // Real Lua adapter failure paths return to native Retry/Cancel; no replay or consumption.
  for (int failure = 0; failure < 4; ++failure) {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    wifi(a, true);
    auto retained = a.launch_;
    if (failure == 0) HostRunner::initOk = false;
    if (failure == 1) HostRunner::loadOk = false;
    if (failure == 2) HostRunner::cooperative = false;
    if (failure == 3) coordinator::failAllocation<HostRunner> = true;
    auto& child = browser(a);
    assert(child.finished && retained->state() == LaunchState::Pending);
    pop(a);
    assert(a.browserFailed_ && !a.child && nativePending && resolutions == 0 && !HalLuaHttp::connected);
    const int before = resolverCalls;
    for (int i = 0; i < 5; ++i) idle(a);
    assert(!a.child && resolverCalls == before && nativePending && resolutions == 0);
  }
  // An optout changed after selector scheduling is still enforced by the real Lua child.
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    wifi(a, true);
    HalLuaHttp::enabled = false;
    auto& child = browser(a);
    assert(child.finished && a.launch_->state() == LaunchState::Pending && HostRunner::loads == 0);
    pop(a);
    assert(a.browserFailed_ && nativePending && !HalLuaHttp::connected && resolutions == 0);
  }
  // Async cache/startup failure is signaled by actual shared state; child cleans before callback.
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    wifi(a, true);
    auto& child = browser(a);
    auto retained = a.launch_;
    assert(retained->failAttempt(HostRunner::live->attempt, LaunchError::ScriptFailed));
    child.loop();
    assert(child.finished);
    pop(a);
    assert(a.browserFailed_ && nativePending && retained->state() == LaunchState::Pending && !HalLuaHttp::connected);
  }
  // ACK only after taking one owned URL; native clear once, no consume at scheduling.
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    assert(resolveAttempts == 0 && nativePending);
    wifi(a, true);
    auto& child = browser(a);
    (void)child;
    auto retained = a.launch_;
    auto attempt = HostRunner::live->attempt;
    assert(retained->take(attempt));
    assert(retained->acknowledge(attempt));
    assert(nativePending && resolutions == 0);
    pop(a);
    assert(resolutions == 1 && cancellations == 0 && !nativePending && a.leaving_ && !HalLuaHttp::connected);
    a.onExit();
    assert(resolutions == 1 && cancellations == 0 && retained->state() == LaunchState::Acknowledged);
  }
  // Whole-stack Home replacement destroys child before parent without invoking result callback.
  for (bool acknowledged : {false, true}) {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    wifi(a, true);
    browser(a);
    auto retained = a.launch_;
    if (acknowledged) {
      const auto token = HostRunner::live->attempt;
      assert(retained->take(token));
      assert(retained->acknowledge(token));
    }
    a.child->onExit();
    a.child.reset();
    a.resultCallback = {};
    a.onExit();
    assert(!HalLuaHttp::connected && !nativePending);
    assert(resolutions == int(acknowledged) && cancellations == int(!acknowledged));
  }
  // A later optout keeps an earlier failed attempt's same payload, without selector/child replay.
  {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    wifi(a, false, true);
    auto* request = a.launch_.operator->();
    HalLuaHttp::enabled = false;
    const int calls = resolverCalls;
    a.startBrowserHandoff();
    assert(!a.child && a.launch_.operator->() == request && a.launch_->state() == LaunchState::Pending &&
           nativePending && resolverCalls == calls);
    a.onExit();
    assert(cancellations == 1);
  }
  // Back/Home cancels retained one-shot state, but native resolution waits for radio stop.
  for (bool home : {false, true}) {
    reset();
    AirDropReceiveActivity a(renderer, input);
    a.startBrowserHandoff();
    auto retained = a.launch_;
    // Remove the selector through its real cancellation/cleanup ordering first.
    wifi(a, false, true);
    receiverStopped = false;
    if (home)
      assert(a.handleHomeGesture());
    else
      a.beginLeaving(false);
    assert(a.leaving_ && a.homeRequested_ == home && retained->state() == LaunchState::Cancelled && nativePending &&
           cancellations == 0);
    receiverStopped = true;
    a.onExit();
    assert(cancellations == 1 && !nativePending && !HalLuaHttp::connected);
  }
  std::puts(
      "PASS actual native coordinator callbacks + Lua activity: optout/preflight, selector/package/allocation "
      "failures, retained retry, no replay, ACK once, ownership cleanup, Back/Home");
}
