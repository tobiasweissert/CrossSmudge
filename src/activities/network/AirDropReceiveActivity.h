#pragma once
#include "AppCapabilities.h"
#if CROSSINK_APP_CAP_AIRDROP
#include <cstdint>
#include <string>
#include "activities/Activity.h"
#include "activities/apps/lua/AppPackage.h"
#include "activities/ScreenTransitionRefresh.h"
#include "components/UiAppHost.h"
#include "components/AirDropConsentInput.h"
#include "network/AirDropReceiver.h"
#include <browser/BrowserLaunch.h>

// Foreground coordinator only. Receiver has process lifetime and owns all workers;
// activity destruction never frees a live socket, callback context or radio owner.
class AirDropReceiveActivity final : public Activity {
 public:
  AirDropReceiveActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string returnBookPath = {});
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
  bool blocksGlobalInput() const override { return true; }
  bool handleHomeGesture() override;
  bool deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN action) override;

 private:
  using UiHost = UiAppHost<4, 4>;
  using UiApp = UiHost::App;
  UiHost ui_;
  ScreenTransitionRefresh refresh_;
  // Fixed buffers live in the heap-owned Activity, never on a task's small stack.
  airdrop::Status status_{};
  airdrop::OfferIdentity paintedIdentity_{};
  airdrop::OfferIdentity routedIdentity_{};
  airdrop::input::ConsentGate consentInput_;
  uint32_t routedEpoch_ = 0;
  std::string returnBookPath_;
  airdrop::browser::LaunchHandle launch_;
  AppPackage browserPackage_;  // Reused for installed-package checks; avoids a large stack object.
  char textBuffer_[256]{};
  uint64_t lastPaintRequestMs_ = 0;
  bool paintedPending_ = false;
  bool decisionSent_ = false;
  bool leaving_ = false;
  bool homeRequested_ = false;
  bool powerDeferred_ = false;
  bool childPending_ = false;
  bool browserAttempted_ = false;
  bool browserFailed_ = false;
  bool wifiOwned_ = false;
  bool startFailed_ = false;
  bool radioStopped_ = false;
  bool handoffResolved_ = false;

  static void screen(UiApp::ScreenType& screen, void* user);
  static void action(const freeink::ui::ActionEvent& event, void* user);
  static bool sameIdentity(const airdrop::OfferIdentity& a, const airdrop::OfferIdentity& b);
  void buildScreen(UiApp::ScreenType& screen);
  void decide(bool accept);
  bool routeConsentInput();
  void beginLeaving(bool home);
  void startBrowserHandoff();
  void openBrowserChild();
  void setBrowserFailed();
  void resolveHandoff(bool acknowledged);
  void retryReceiver();
};
#endif
