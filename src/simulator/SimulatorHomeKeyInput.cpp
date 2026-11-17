#ifdef SIMULATOR

#include "SimulatorHomeKeyInput.h"

#include <SDL.h>

SimulatorHomeKeyInput simulatorHomeKeyInput;

void SimulatorHomeKeyInput::update() {
  tappedThisFrame = false;
  longPressedThisFrame = false;

}

void SimulatorHomeKeyInput::updateState(const bool pressed, const uint32_t now) {
  if (pressed && !wasPressed) {
    pressedAt = now;
    longPressReported = false;
  } else if (pressed && !longPressReported && now - pressedAt >= LONG_PRESS_MS) {
    longPressedThisFrame = true;
    longPressReported = true;
  } else if (!pressed && wasPressed && !longPressReported) {
    tappedThisFrame = true;
  }
  wasPressed = pressed;
}

void SimulatorHomeKeyInput::injectTap() { injectedTap = true; }

void SimulatorHomeKeyInput::injectLongPress() { injectedLongPress = true; }

bool SimulatorHomeKeyInput::verifyTimingContract() {
  SimulatorHomeKeyInput input;

  input.updateState(false, 0);
  input.updateState(true, 10);
  input.updateState(false, 100);
  if (!input.wasTapped() || input.wasLongPressed()) return false;

  input.tappedThisFrame = false;
  input.updateState(true, 200);
  input.updateState(true, 899);
  if (input.wasLongPressed()) return false;
  input.updateState(true, 900);
  if (input.wasTapped() || !input.wasLongPressed()) return false;

  input.longPressedThisFrame = false;
  input.updateState(false, 910);
  return !input.wasTapped() && !input.wasLongPressed();
}

#endif
