#include "AirDropReceiveActivity.h"
#if CROSSINK_APP_CAP_AIRDROP
#include <I18n.h>
#include <Memory.h>
#include <HalLuaHttp.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include "activities/apps/lua/AppPackage.h"
#include "activities/apps/lua/LuaAppActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;
namespace {
constexpr fui::ActionId Accept = 1, Decline = 2, Cancel = 3, Retry = 4;
constexpr uint64_t PaintIntervalMs = 1200;
}

AirDropReceiveActivity::AirDropReceiveActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             std::string returnBookPath)
    : Activity("AirDropReceive", renderer, mappedInput), ui_(renderer), returnBookPath_(std::move(returnBookPath)) {}

bool AirDropReceiveActivity::sameIdentity(const airdrop::OfferIdentity& a, const airdrop::OfferIdentity& b) {
  return a.generation == b.generation && a.session == b.session && a.offer == b.offer &&
         a.connection == b.connection && a.interfaceIndex == b.interfaceIndex &&
         std::memcmp(a.peer, b.peer, sizeof(a.peer)) == 0;
}

void AirDropReceiveActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock(*this);
    ui_.reset();
    ui_.app.on(Accept, &AirDropReceiveActivity::action, this);
    ui_.app.on(Decline, &AirDropReceiveActivity::action, this);
    ui_.app.on(Cancel, &AirDropReceiveActivity::action, this);
    ui_.app.on(Retry, &AirDropReceiveActivity::action, this);
    ui_.app.setScreen(&AirDropReceiveActivity::screen, this);
  }
  // ActivityManager publishes the activity before onEnter. Keep shared UI
  // initialization protected, but admission/allocation outside RenderLock.
  const bool started = airdrop::Receiver::instance().begin();
  {
    RenderLock lock(*this);
    startFailed_ = !started;
    airdrop::Receiver::instance().snapshot(status_);
    radioStopped_ = airdrop::Receiver::instance().stopped();
  }
  if (!started) LOG_ERR("AIRDROP", "Native receiver start rejected");
  requestUpdate();
}

void AirDropReceiveActivity::resolveHandoff(const bool acknowledged) {
  if (!handoffResolved_ && status_.offer.kind == airdrop::OfferKind::Url &&
      airdrop::Receiver::instance().resolvePendingUrl(status_.identity, acknowledged)) handoffResolved_ = true;
}

void AirDropReceiveActivity::onExit() {
  Activity::onExit();
  ui_.closeRouting();
  leaving_ = true;
  paintedPending_ = false;
  // onExit holds RenderLock. Revoke admission only; the persistent owner joins itself.
  auto& receiver = airdrop::Receiver::instance();
  receiver.requestStop();
  if (receiver.stopped()) receiver.reap();  // Already joined; no waits or live resource destruction.
  if (launch_) {
    const bool acknowledged = launch_->state() == airdrop::browser::LaunchState::Acknowledged;
    if (!acknowledged) launch_->cancel();
    resolveHandoff(acknowledged);
  } else if (status_.offer.kind == airdrop::OfferKind::Url) {
    resolveHandoff(false);
  }
  if (wifiOwned_ && receiver.stopped()) HalLuaHttp::disconnectWifi();
  wifiOwned_ = false;
}

void AirDropReceiveActivity::beginLeaving(const bool home) {
  {
    RenderLock lock(*this);
    leaving_ = true;
    homeRequested_ = homeRequested_ || home;
    paintedPending_ = false;
  }
  if (launch_) launch_->cancel();
  resolveHandoff(false);
  airdrop::Receiver::instance().requestStop();
  requestUpdate();
}

bool AirDropReceiveActivity::handleHomeGesture() {
  beginLeaving(true);
  return true;
}

bool AirDropReceiveActivity::deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN) {
  if (airdrop::Receiver::instance().stopped()) {
    powerDeferred_ = false;
    return false;
  }
  powerDeferred_ = true;
  beginLeaving(false);
  return true;
}

bool AirDropReceiveActivity::routeConsentInput() {
  bool touch = false, confirm = false;
  {
    RenderLock lock(*this);
    const auto snapshot = touchSnapshotFrom(mappedInput);
    if (!paintedPending_ || !consentInput_.consume(airdrop::input::capture.frame(),
        mappedInput.isScreenContactActive(), snapshot.touchPressed, snapshot.touchReleased,
        mappedInput.isPhysicalPressed(MappedInputManager::Button::Confirm),
        mappedInput.wasPressed(MappedInputManager::Button::Confirm), touch, confirm)) return false;
    routedEpoch_ = consentInput_.epoch();
    routedIdentity_ = paintedIdentity_;
  }
  // SDK routing/handlers may wait through a paint. decide() rechecks this
  // captured token under RenderLock rather than assigning a new epoch there.
  fui::ActionEvent event{};
  const bool routed = touch && ui_.routeTouch(mappedInput, event) && event;
  if (!routed && confirm) {
    mappedInput.suppressNextConfirmRelease();
    decide(true);
  }
  { RenderLock lock(*this); routedEpoch_ = 0; }
  return routed || confirm;
}

void AirDropReceiveActivity::decide(const bool accept) {
  {
    RenderLock lock(*this);
    if (leaving_ || childPending_ || decisionSent_ || !paintedPending_ || !routedEpoch_ ||
        routedEpoch_ != consentInput_.epoch() || routedEpoch_ != airdrop::input::capture.begin() ||
        !sameIdentity(routedIdentity_, paintedIdentity_) ||
        status_.state != airdrop::State::Pending || !sameIdentity(paintedIdentity_, status_.identity) ||
        status_.nowMs >= status_.deadlineMs) return;
    decisionSent_ = true;
    paintedPending_ = false;
    // Receiver::decide is a bounded critical-section queue write, with no I/O
    // or render callback. Keep the epoch stable through final identity/deadline
    // validation and delivery; receiver snapshot already uses this lock order.
    if (!airdrop::Receiver::instance().decide(routedIdentity_, accept)) {
      LOG_ERR("AIRDROP", "Offer decision rejected or decision queue unavailable");
    }
  }
  requestUpdate();
}

void AirDropReceiveActivity::setBrowserFailed() {
  {
    RenderLock lock(*this);
    browserFailed_ = true;
    childPending_ = false;
  }
  LOG_ERR("AIRDROP", "Browser handoff incomplete; URL retained for retry or cancellation");
  requestUpdate();
}

void AirDropReceiveActivity::startBrowserHandoff() {
  auto& receiver = airdrop::Receiver::instance();
  if (leaving_ || childPending_ || !receiver.stopped() || status_.offer.kind != airdrop::OfferKind::Url) return;
  browserAttempted_ = true;
  if (!HalLuaHttp::networkEnabled()) { setBrowserFailed(); return; }
  AppPackage::BrowserResolveError resolution;
  if (!AppPackage::resolveInstalledBrowser(browserPackage_, resolution)) { setBrowserFailed(); return; }
  { RenderLock lock(*this); browserFailed_ = false; }
  if (!launch_) {
    // One bounded owned URL must outlive this loop and nested WiFi/Lua activities.
    auto request = makeUniqueNoThrow<airdrop::browser::LaunchRequest>();
    if (!request || !request->prepare(status_.offer.url)) { setBrowserFailed(); return; }
    launch_ = airdrop::browser::LaunchHandle(std::move(request));
  }
  if (launch_->state() == airdrop::browser::LaunchState::Acknowledged) {
    resolveHandoff(true);
    beginLeaving(false);
    return;
  }
  if (HalLuaHttp::wifiConnected()) { openBrowserChild(); return; }
  auto selector = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true);
  if (!selector) { setBrowserFailed(); return; }
  wifiOwned_ = true;
  childPending_ = true;
  startActivityForResult(std::move(selector), [this](const ActivityResult& result) {
    childPending_ = false;
    if (result.isCancelled || !HalLuaHttp::wifiConnected()) { setBrowserFailed(); return; }
    openBrowserChild();
  });
}

void AirDropReceiveActivity::openBrowserChild() {
  if (!airdrop::Receiver::instance().stopped() || leaving_ || !launch_) return;
  AppPackage::BrowserResolveError error;
  if (!AppPackage::resolveInstalledBrowser(browserPackage_, error)) { setBrowserFailed(); return; }
  auto browser = makeUniqueNoThrow<LuaAppActivity>(renderer, mappedInput, browserPackage_.path, browserPackage_.name,
                                                 browserPackage_.entryScript, launch_, wifiOwned_);
  if (!browser) { setBrowserFailed(); return; }
  wifiOwned_ = false; // Explicit ownership passes to the child only after successful allocation.
  childPending_ = true;
  startActivityForResult(std::move(browser), [this](const ActivityResult&) {
    childPending_ = false;
    if (launch_ && launch_->state() == airdrop::browser::LaunchState::Acknowledged) {
      resolveHandoff(true);
      beginLeaving(false);
    } else setBrowserFailed();
  });
}

void AirDropReceiveActivity::loop() {
  auto& receiver = airdrop::Receiver::instance();
  uint64_t previousRevision;
  airdrop::State previousState;
  bool identityChanged, previousOwnership;
  {
    RenderLock lock(*this);
    previousRevision = status_.revision;
    previousState = status_.state;
    previousOwnership = status_.ownershipUncertain;
    const auto previousIdentity = status_.identity;
    receiver.snapshot(status_);
    radioStopped_ = receiver.stopped();
    identityChanged = !sameIdentity(previousIdentity, status_.identity);
    if (identityChanged || status_.state != airdrop::State::Pending) {
      paintedPending_ = false;
      decisionSent_ = false;
    }
  }
  // Reclaim stopped protocol/TLS/decoder workspaces outside RenderLock before Lua/WiFi.
  if (receiver.stopped()) receiver.reap();
  if (mappedInput.wasHomeGesture()) { handleHomeGesture(); return; }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) || mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    mappedInput.suppressNextBackRelease();
    beginLeaving(false);
  }
  if (leaving_) {
    if (receiver.stopped() && !powerDeferred_) activityManager.returnFromAirDrop(returnBookPath_, homeRequested_);
  } else if (status_.state == airdrop::State::UrlReady && receiver.stopped() && !browserAttempted_) {
    startBrowserHandoff();
  } else if (status_.state == airdrop::State::Pending) {
    if (routeConsentInput()) return;
  } else {
    fui::ActionEvent event{};
    if (ui_.routeTouch(mappedInput, event) && event) return;
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      mappedInput.suppressNextConfirmRelease();
      if (browserFailed_) startBrowserHandoff();
      else if (startFailed_ || status_.state == airdrop::State::Error || status_.state == airdrop::State::Stopped)
        retryReceiver();
    }
  }
  if (previousState != status_.state || identityChanged || previousOwnership != status_.ownershipUncertain ||
      ((previousRevision != status_.revision || status_.state == airdrop::State::Pending) &&
       status_.nowMs - lastPaintRequestMs_ >= PaintIntervalMs)) {
    lastPaintRequestMs_ = status_.nowMs;
    requestUpdate();
  }
}

void AirDropReceiveActivity::retryReceiver() {
  auto& receiver = airdrop::Receiver::instance();
  if (leaving_ || status_.publicationUncertain || !receiver.stopped()) {
    receiver.requestStop();
    requestUpdate();
    return;
  }
  const bool started = receiver.begin();
  {
    RenderLock lock(*this);
    startFailed_ = !started;
    radioStopped_ = receiver.stopped();
    paintedPending_ = false;
    decisionSent_ = false;
    receiver.snapshot(status_);
  }
  if (!started) LOG_ERR("AIRDROP", "Explicit native receiver retry rejected");
  requestUpdate();
}

void AirDropReceiveActivity::action(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<AirDropReceiveActivity*>(user);
  if (event.action == Accept) self->decide(true);
  else if (event.action == Decline) self->decide(false);
  else if (event.action == Cancel) self->beginLeaving(false);
  else if (event.action == Retry) {
    if (self->browserFailed_) self->startBrowserHandoff();
    else self->retryReceiver();
  }
}

void AirDropReceiveActivity::screen(UiApp::ScreenType& screen, void* user) {
  static_cast<AirDropReceiveActivity*>(user)->buildScreen(screen);
}

void AirDropReceiveActivity::buildScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  // FreeInkUI's target derives safeArea from getOrientedViewableTRBL().
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(header.y + header.height),
      static_cast<int16_t>(metrics.contentSidePadding), static_cast<int16_t>(metrics.buttonHintsHeight),
      static_cast<int16_t>(metrics.contentSidePadding)});
  screen.spacer(theme.spaceMd);
  auto text = [&](const char* value, uint8_t lines = 1, bool small = false, bool bold = false) {
    auto style = small ? theme.smallText : theme.bodyText;
    style.align = fui::TextAlign::Center; style.maxLines = lines; style.bold = bold;
    const int16_t lineHeight = screen.target().lineHeight(style.font);
    const int16_t gap = status_.state == airdrop::State::Pending && !leaving_ ? theme.spaceSm : theme.spaceMd;
    screen.target().text(screen.takeTop(static_cast<int16_t>(lineHeight * lines), gap), value, style);
  };
  auto completeLines = [&](const char* value, const bool bold = false) {
    auto style = theme.smallText; style.bold = bold;
    const int width = std::max<int>(1, screen.body().width);
    const int measured = screen.target().measureText(style.font, value, style).width;
    // Rounded total width can underestimate discrete character/word wrapping by a
    // line. Reserve that slack so a long host or filename is never merely an ellipsis.
    const int lines = (measured + width - 1) / width + (measured > width ? 1 : 0);
    return static_cast<uint8_t>(std::clamp<int>(lines, 1, 8));
  };
  auto button = [&](fui::ActionId id, const char* label, fui::Rect rect, bool enabled = true) {
    fui::ButtonProps props;
    props.action = id; props.label = label; props.inputMask = fui::InputTouch;
    props.text = theme.bodyText; props.text.align = fui::TextAlign::Center; props.enabled = enabled;
    screen.button(props, rect);
  };
  const bool pending = status_.state == airdrop::State::Pending && !leaving_;
  const bool stopping = leaving_ || status_.state == airdrop::State::Stopping;
  auto actions = screen.takeBottom(theme.rowHeight);
  if (pending) {
    const int16_t gap = theme.spaceMd;
    const int16_t width = static_cast<int16_t>((actions.width - gap) / 2);
    button(Decline, tr(STR_AIRDROP_DECLINE), fui::Rect{actions.x, actions.y, width, actions.height}, !decisionSent_);
    button(Accept, tr(STR_AIRDROP_ACCEPT), fui::Rect{static_cast<int16_t>(actions.x + width + gap), actions.y,
           width, actions.height}, !decisionSent_ && status_.nowMs < status_.deadlineMs);
  } else if (browserFailed_ || status_.ownershipUncertain ||
             (radioStopped_ && !leaving_ && !status_.publicationUncertain &&
              (startFailed_ || status_.state == airdrop::State::Error || status_.state == airdrop::State::Stopped))) {
    const int16_t width = static_cast<int16_t>((actions.width - theme.spaceMd) / 2);
    button(Cancel, tr(STR_CANCEL), fui::Rect{actions.x, actions.y, width, actions.height});
    button(Retry, (browserFailed_ || radioStopped_) ? tr(STR_AIRDROP_RETRY) : tr(STR_AIRDROP_RETRY_STOP),
           fui::Rect{static_cast<int16_t>(actions.x + width + theme.spaceMd), actions.y, width, actions.height});
  } else button(Cancel, tr(STR_CANCEL), actions);

  if (status_.publicationUncertain) {
    text(tr(STR_AIRDROP_FAILED), 2, false, true); text(tr(STR_AIRDROP_PUBLICATION_UNCERTAIN), 4, true);
    if (status_.ownershipUncertain) text(tr(STR_AIRDROP_RETAINED), 3, true);
  } else if (status_.ownershipUncertain) {
    text(tr(STR_AIRDROP_FAILED), 2, false, true); text(tr(STR_AIRDROP_RETAINED), 4, true);
  } else if (browserFailed_) {
    text(tr(STR_AIRDROP_URL_READY), 1, false, true); text(tr(STR_AIRDROP_BROWSER_FAILED), 4, true);
  } else if (stopping) {
    text(tr(STR_AIRDROP_STOPPING), 2, false, true); text(tr(STR_AIRDROP_STOPPING_HINT), 3, true);
  } else if (pending) {
    text(tr(STR_AIRDROP_INCOMING), 1, false, true);
    std::snprintf(textBuffer_, sizeof(textBuffer_), tr(STR_AIRDROP_UNVERIFIED_SENDER), status_.offer.sender);
    text(textBuffer_, completeLines(textBuffer_), true);
    if (status_.offer.kind == airdrop::OfferKind::Url) {
      text(status_.offer.host, completeLines(status_.offer.host, true), true, true);
      // Reserve the deadline before spending remaining height on the optional URL path.
      const int lineHeight = screen.target().lineHeight(theme.smallText.font);
      const int remaining = screen.body().height - lineHeight - theme.spaceMd * 2;
      const int urlLines = std::clamp<int>(remaining / std::max(1, lineHeight), 0, 3);
      if (urlLines > 0) text(status_.offer.url, static_cast<uint8_t>(urlLines), true);
    } else {
      text(status_.offer.filename, completeLines(status_.offer.filename, true), true, true);
      std::snprintf(textBuffer_, sizeof(textBuffer_), tr(STR_AIRDROP_FILE_BYTES), static_cast<unsigned long>(status_.offer.fileSize)); text(textBuffer_, 1, true);
    }
    const uint64_t seconds = status_.deadlineMs > status_.nowMs ? (status_.deadlineMs - status_.nowMs + 999) / 1000 : 0;
    std::snprintf(textBuffer_, sizeof(textBuffer_), tr(STR_AIRDROP_DECIDE_SECONDS), static_cast<unsigned long>(seconds));
    text(textBuffer_, 1, true);
  } else if (status_.state == airdrop::State::Receiving || status_.state == airdrop::State::Publishing) {
    text(status_.state == airdrop::State::Publishing ? tr(STR_AIRDROP_VERIFYING) : tr(STR_AIRDROP_RECEIVING), 2, false, true);
    text(status_.offer.filename, 2, true);
    fui::ProgressBarProps progress;
    progress.value = static_cast<int32_t>(std::min<uint64_t>(status_.committedBytes, airdrop::MaxFileBytes));
    progress.max = static_cast<int32_t>(std::max<uint32_t>(1, status_.offer.fileSize));
    fui::progressBar(screen.frame(), screen.takeTop(theme.spaceLg, theme.spaceMd), progress);
    std::snprintf(textBuffer_, sizeof(textBuffer_), tr(STR_AIRDROP_PROGRESS), static_cast<unsigned long>(status_.wireBytes),
                  static_cast<unsigned long>(status_.payloadBytes), static_cast<unsigned long>(status_.committedBytes));
    text(textBuffer_, 3, true);
  } else if (status_.state == airdrop::State::Success) {
    text(tr(STR_AIRDROP_RECEIVED), 2, false, true); text(status_.savedPath, 3, true);
  } else if (status_.state == airdrop::State::UrlReady) {
    text(tr(STR_AIRDROP_URL_READY), 1, false, true); text(status_.offer.host, 2, false);
    text(tr(STR_AIRDROP_CONNECTING_BROWSER), 3, true);
  } else if (startFailed_ || status_.state == airdrop::State::Error) {
    text(tr(STR_AIRDROP_FAILED), 2, false, true);
  } else if (status_.state == airdrop::State::Stopped) {
    text(tr(STR_AIRDROP_STOPPED), 2, false, true);
  } else {
    text(status_.state == airdrop::State::Starting ? tr(STR_AIRDROP_STARTING) : tr(STR_AIRDROP_LISTENING), 2, false, true);
    text(tr(STR_AIRDROP_ACTIVE_ONLY), 3, true);
  }
}

void AirDropReceiveActivity::render(RenderLock&&) {
  airdrop::input::capture.beginPaint();
  paintedPending_ = false;
  renderer.clearScreen();
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  TouchHeaderBackButton::draw(renderer, ui_.target, header, tr(STR_AIRDROP_TITLE), false);
  ui_.closeRouting();
  ui_.render();
  const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), status_.state == airdrop::State::Pending ? tr(STR_AIRDROP_ACCEPT) : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(refresh_.modeFor(static_cast<uint8_t>(status_.state)));
  consentInput_.painted(airdrop::input::capture.completePaint());
  if (status_.state == airdrop::State::Pending && !leaving_ && !decisionSent_) {
    paintedIdentity_ = status_.identity;
    paintedPending_ = true;
  }
}
#endif
