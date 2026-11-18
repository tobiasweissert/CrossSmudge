// Production LuaAppActivityAdapter with only VM and device/network boundaries replaced.
#include <GfxRenderer.h>
#include <MappedInputManager.h>

#include <cassert>
#include <memory>
#include <string>

#include "BrowserLaunch.h"
struct HostRunner {
  inline static HostRunner* live = nullptr;
  inline static bool initOk = true, network = true, cooperative = true, loadOk = true;
  bool error = false;
  std::string message;
  airdrop::browser::LaunchHandle launch;
  uint32_t attempt = 0;
  HostRunner(GfxRenderer&, MappedInputManager&, const std::string&, const std::string&) { live = this; }
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
    if (!loadOk) {
      error = true;
      message = "script failed";
      if (launch) launch->failAttempt(attempt, airdrop::browser::LaunchError::ScriptFailed);
    }
    return loadOk;
  }
  bool networkEnabled() { return network; }
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
#include "LuaAppActivity.h"
using Adapter = LuaAppActivityAdapter<HostRunner>;
using namespace airdrop::browser;
static LaunchHandle pending() {
  auto x = std::unique_ptr<LaunchRequest>(new LaunchRequest);
  assert(x->prepare("https://example.com/a#f"));
  return LaunchHandle(std::move(x));
}
int main() {
  GfxRenderer renderer;
  MappedInputManager input;
  {
    HalLuaHttp::connected = true;
    HalLuaHttp::disconnects = 0;
    Adapter ordinary(renderer, input, "apps/browser", "Browser");
    ordinary.onEnter();
    ordinary.onExit();
    assert(HalLuaHttp::disconnects == 0);
  }
  {
    auto launch = pending();
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    assert(launch->state() == LaunchState::Offered);
    assert(HostRunner::live->launch);
    app.onExit();
    assert(launch->state() == LaunchState::Pending && launch->error() == LaunchError::ExitedBeforeAck);
    assert(!HalLuaHttp::connected && HalLuaHttp::disconnects == 1);
  }
  {
    auto launch = pending();
    HostRunner::initOk = false;
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    assert(launch->state() == LaunchState::Pending && launch->error() == LaunchError::InitFailed);
    app.onExit();
    HostRunner::initOk = true;
  }
  {
    auto launch = pending();
    HostRunner::network = false;
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    assert(app.finished && launch->state() == LaunchState::Pending &&
           launch->error() == LaunchError::NetworkUnavailable);
    app.onExit();
    HostRunner::network = true;
  }
  {
    auto launch = pending();
    HostRunner::cooperative = false;
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    assert(app.finished && launch->state() == LaunchState::Pending);
    app.onExit();
    HostRunner::cooperative = true;
  }
  {
    auto launch = pending();
    HostRunner::loadOk = false;
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    assert(app.finished && launch->state() == LaunchState::Pending && launch->error() == LaunchError::ScriptFailed);
    app.onExit();
    HostRunner::loadOk = true;
  }
  {
    auto launch = pending();
    HalLuaHttp::connected = false;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch);
    app.onEnter();
    app.loop();
    assert(Activity::resultCallback);
    Activity::resultCallback(ActivityResult{true});
    assert(app.finished && launch->state() == LaunchState::Pending &&
           launch->error() == LaunchError::NetworkUnavailable);
    Activity::resultCallback = {};
    app.onExit();
  }
  {
    auto launch = pending();
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    auto& runner = *HostRunner::live;
    assert(runner.launch->failAttempt(runner.attempt, LaunchError::ScriptFailed));
    app.loop();
    assert(app.finished);
    app.onExit();
  }
  {
    auto launch = pending();
    HalLuaHttp::connected = true;
    Adapter app(renderer, input, "apps/browser", "Browser", "main.lua", launch, true);
    app.onEnter();
    auto& runner = *HostRunner::live;
    assert(runner.launch->take(runner.attempt));
    assert(runner.launch->acknowledge(runner.attempt));
    app.onExit();
    assert(launch->state() == LaunchState::Acknowledged);
    Adapter stale(renderer, input, "apps/browser", "Browser", "main.lua", launch);
    stale.onEnter();
    assert(stale.finished);
    stale.onExit();
  }
}
