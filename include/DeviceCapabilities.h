#pragma once

#include <HalGPIO.h>

#include "AppCapabilities.h"

inline bool deviceHasEdgeSideButtons(const HalGPIO& gpio) {
#ifdef SIMULATOR
  (void)gpio;
  return false;
#else
  return gpio.hasEdgeSideButtons();
#endif
}

inline bool deviceUsesSideButtonHintGutters(const HalGPIO& gpio) {
  if (!deviceHasEdgeSideButtons(gpio)) return false;
#if CROSSSMUDGE_APP_CAP_TOUCH
  return !gpio.hasTouch();
#else
  return true;
#endif
}

// Touch readers can safely dedicate both side buttons to a chord because Back
// and Confirm remain available on-screen.
inline bool deviceSupportsSideButtonChord(const HalGPIO& gpio) { return gpio.hasTouch(); }
