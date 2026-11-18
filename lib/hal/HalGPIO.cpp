#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <HalGPIO.h>
#include <Logging.h>
#include <PowerManager.h>
#include <esp_sleep.h>

#include <algorithm>

#if FREEINK_MCU_S3
#include <soc/usb_serial_jtag_reg.h>
#endif


// Global HalGPIO instance
HalGPIO gpio;

namespace {
constexpr unsigned long BUTTON_DEBOUNCE_REPOLL_MS = 6;

}  // namespace

void HalGPIO::begin() {
  inputMgr.begin();
}

void HalGPIO::update() {
  inputMgr.update();
  if (inputMgr.isDebouncePending()) {
    // The SDK commits a state change after it remains stable for more than
    // 5 ms. Re-poll before the low-power loop's next ~100 ms sample so short
    // physical button presses are not discarded.
    delay(BUTTON_DEBOUNCE_REPOLL_MS);
    inputMgr.update();
  }

  usbStateChanged = false;
  const bool connected = isUsbConnected();
  usbStateChanged = (connected != lastUsbConnected);
  lastUsbConnected = connected;
  usbStateSampled = true;
}

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

unsigned long HalGPIO::getPowerButtonHeldTime() const { return inputMgr.getPowerButtonHeldTime(); }

#if CROSSSMUDGE_APP_CAP_TOUCH
bool HalGPIO::hasTouch() const { return inputMgr.hasTouch(); }

bool HalGPIO::supportsMultiTouch() const { return inputMgr.supportsMultiTouch(); }
bool HalGPIO::isTouchPressed() const { return inputMgr.isTouchPressed(); }

HalGPIO::TouchSnapshot HalGPIO::getTouchSnapshot() const {
  TouchSnapshot result;
  if (!supportsMultiTouch()) return result;

  const auto source = inputMgr.getTouchSnapshot();
  const auto& touch = BoardConfig::ACTIVE.touch;
  const uint16_t width = touch.rawMaxX > touch.rawMinX ? touch.rawMaxX - touch.rawMinX : 1;
  const uint16_t height = touch.rawMaxY > touch.rawMinY ? touch.rawMaxY - touch.rawMinY : 1;
  result.count = std::min<uint8_t>(source.count, TouchSnapshot::MAX_CONTACTS);
  result.reportedCount = source.reportedCount;
  for (uint8_t i = 0; i < result.count; ++i) {
    const auto& point = source.points[i];
    result.contacts[i].id = point.id;
    result.contacts[i].nx = std::clamp(static_cast<float>(point.point.x) / width, 0.0f, 1.0f);
    result.contacts[i].ny = std::clamp(static_cast<float>(point.point.y) / height, 0.0f, 1.0f);
  }
  return result;
}

bool HalGPIO::wasCompletedMultiTouchSwipe(CompletedMultiTouchSwipe& swipe) const {
  if (!supportsMultiTouch()) return false;
  return inputMgr.wasMultiTouchSwipe(swipe.contactCount, swipe.nxStart, swipe.nyStart, swipe.nxEnd, swipe.nyEnd,
                                     swipe.durationMs);
}

bool HalGPIO::wasCompletedMultiTouchRotation(CompletedMultiTouchRotation& rotation) const {
  if (!supportsMultiTouch()) return false;
  return inputMgr.wasMultiTouchRotation(rotation.degrees, rotation.nxCenter, rotation.nyCenter, rotation.durationMs);
}

bool HalGPIO::hasHomeKey() const { return BoardConfig::hasHomeKey(); }

bool HalGPIO::wasHomeKeyPressed() const { return inputMgr.wasHomeKeyPressed(); }

bool HalGPIO::wasHomeKeyTapped() const { return inputMgr.wasHomeKeyTapped(); }

bool HalGPIO::wasHomeKeyLongPressed() const { return inputMgr.wasHomeKeyLongPressed(); }

bool HalGPIO::wasTouchTap(float& nx, float& ny) const { return inputMgr.wasTouchTap(nx, ny); }

bool HalGPIO::wasTouchDown(float& nx, float& ny) const { return inputMgr.wasTouchPressedAt(nx, ny); }

bool HalGPIO::wasTouchReleased() const { return inputMgr.wasTouchReleased(); }

bool HalGPIO::isTouchTapCandidate(float& nx, float& ny, unsigned long& heldMs) const {
  return inputMgr.isTouchTapCandidate(nx, ny, heldMs);
}

bool HalGPIO::wasTouchLongPress(float& nx, float& ny) const { return inputMgr.wasTouchLongPress(nx, ny); }

void HalGPIO::suppressTouchContact() { inputMgr.suppressTouchContact(); }

bool HalGPIO::isTouchHeldAt(float& nx, float& ny) const { return inputMgr.isTouchHeldAt(nx, ny); }

unsigned long HalGPIO::lastTouchHeldMs() const { return inputMgr.lastTouchHeldMs(); }

bool HalGPIO::wasSwipe(float& nxStart, float& nyStart, float& nxEnd, float& nyEnd) const {
  return inputMgr.wasSwipe(nxStart, nyStart, nxEnd, nyEnd);
}

bool HalGPIO::wasTouchActivity() const { return inputMgr.wasTouchActivity(); }
#endif

void HalGPIO::setSharedConfirmPowerShortPressEmitsPower(const bool enabled) {
  InputManager::setSharedConfirmPowerShortPressEmitsPower(enabled);
}

bool HalGPIO::hasEdgeSideButtons() const { return false; }

bool HalGPIO::verifyPowerButtonWakeup(const bool shortPressWakes) {
  // M5Paper v1.1 reaches setup after a normal wheel click has already been
  // released. Its hardware pull-ups make this ghost-wake debounce unnecessary.
  if (BoardConfig::isPaperMono() || BoardConfig::isM5PaperV11() || BoardConfig::ACTIVE.input.power < 0) {
    return true;
  }

  constexpr unsigned long POWER_WAKE_STABILITY_MS = 10;
  const bool heldAtFirstSample = inputMgr.isPowerButtonPhysicallyPressed();
  const unsigned long sampleStart = millis();
  inputMgr.update();
  while (millis() - sampleStart < POWER_WAKE_STABILITY_MS || inputMgr.isDebouncePending()) {
    delay(1);
    inputMgr.update();
  }
  return shortPressWakes || (heldAtFirstSample && inputMgr.isPowerButtonPhysicallyPressed());
}

#if FREEINK_MCU_S3
// USB host presence via the USB-Serial-JTAG peripheral's SOF frame counter: a
// connected host clocks 1 kHz start-of-frame packets, so the counter advancing
// between polls means a data-capable host is attached. Held for a short window
// so multiple callers within one loop tick (update() plus header renders) all
// see the same answer. Limitation: a data-less wall charger sends no SOFs and
// stays invisible; attach and detach remain observable USB events.
static bool usbHostSofActive() {
  static uint32_t lastFrame = 0;
  static unsigned long lastAdvanceMs = 0;
  static bool seeded = false;
  if (!seeded) {
    // First probe must not fabricate a connection: getWakeupReason() calls this
    // at boot, and a false positive turns a power-button wake (POWERON reset)
    // into AfterUSBPower, which goes straight back to deep sleep — the device
    // never wakes. Seed the counter and wait one SOF period out; a real host
    // clocks SOFs at 1 kHz, so 3 ms guarantees advancement when attached.
    seeded = true;
    lastFrame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG);
    delay(3);
  }
  const uint32_t frame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG);
  if (frame != lastFrame) {
    lastFrame = frame;
    lastAdvanceMs = millis();
    return true;
  }
  return lastAdvanceMs != 0 && millis() - lastAdvanceMs < 1500;
}
#endif

bool HalGPIO::isUsbConnected() const {
  if (BoardConfig::ACTIVE.usbDetect >= 0) {
    return digitalRead(BoardConfig::ACTIVE.usbDetect) == HIGH;
  }
#if FREEINK_MCU_S3
  // Without a VBUS pin, prefer native-USB host traffic, then use charging as a
  // fallback for power-only adapters that do not emit USB frames. Charge
  // termination at 100% can still report disconnected.
  if (usbHostSofActive()) return true;
  static const BatteryMonitor battery;
  static bool sampled = false;
  static bool charging = false;
  static unsigned long lastSampleMs = 0;
  const unsigned long now = millis();
  if (!sampled || now - lastSampleMs >= 500) {
    charging = battery.isCharging();
    lastSampleMs = now;
    sampled = true;
  }
  return charging;
#else
  return false;
#endif
}

bool HalGPIO::isUsbConnectedCached() const { return usbStateSampled ? lastUsbConnected : isUsbConnected(); }

bool HalGPIO::coldBootImpliesPowerButton() const {
  // These boards use a button-energized or otherwise known latch topology, so
  // a no-USB POWERON can be trusted as a power-button boot. Unknown/future
  // topologies must continue booting instead of risking an immediate sleep
  // after flashing or when charge detection is unavailable.
  return BoardConfig::isSticky();
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  const bool usbConnected = isUsbConnected();

  if (resetReason == ESP_RST_DEEPSLEEP &&
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO || wakeupCause == ESP_SLEEP_WAKEUP_EXT1)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected &&
      coldBootImpliesPowerButton()) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
