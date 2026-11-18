#pragma once

#include <atomic>
#include <cstdint>

#include "MappedInputManager.h"

// App-local consent fence. No SDK/OptionPopup policy changes. A completed paint
// is followed by a discarded snapshot and a neutral frame; only a subsequent
// press/contact can activate a mutation. Release-only fallbacks are not consent.
class FileMutationInputGate {
 public:
  void beginPaint() { paintEpoch.fetch_or(1U, std::memory_order_acq_rel); }
  void endPaint() { paintEpoch.fetch_add(1, std::memory_order_release); }

  bool allowInput(MappedInputManager& input) {
    const uint32_t epoch = paintEpoch.load(std::memory_order_acquire);
    if (epoch == 0 || (epoch & 1U)) return false;
    if (epoch != observedEpoch) {
      observedEpoch = epoch;
      waitingForNeutral = true;
      touchArmed = false;
      buttonsArmed = 0;
      return false;
    }
    int x = 0, y = 0;
    const bool down = input.wasScreenTouchDown(x, y);
    const bool tapped = input.wasScreenTapped(x, y);
    if (waitingForNeutral) {
      bool held = input.isScreenContactActive();
      for (const auto button : buttons) held = held || input.isPhysicalPressed(button);
      if (!held && !down && !tapped && !input.wasAnyPressed() && !input.wasAnyReleased() &&
          !input.wasScreenTouchReleased())
        waitingForNeutral = false;
      return false;
    }
    bool allowed = false;
    for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
      const auto bit = static_cast<uint16_t>(1U << i);
      if (input.wasPressed(buttons[i])) {
        buttonsArmed |= bit;
        allowed = true;
      }
      if (input.wasReleased(buttons[i])) {
        allowed = allowed || (buttonsArmed & bit);
        buttonsArmed &= ~bit;
      }
    }
    if (down) {
      touchArmed = true;
      allowed = true;
    }
    if (tapped) {
      allowed = allowed || touchArmed;
      touchArmed = false;
    }
    if (input.wasScreenTouchReleased()) touchArmed = false;
    // Idle frames must reach popup lifecycle handling (Back/outside dismissal
    // is completed by ConfirmationActivity on its following idle loop).
    return allowed || (!down && !tapped && !input.wasAnyPressed() && !input.wasAnyReleased() &&
                       !input.isScreenContactActive() && !input.wasScreenTouchReleased());
  }

 private:
  inline static constexpr MappedInputManager::Button buttons[] = {
      MappedInputManager::Button::Back,  MappedInputManager::Button::Confirm, MappedInputManager::Button::Left,
      MappedInputManager::Button::Right, MappedInputManager::Button::Up,      MappedInputManager::Button::Down,
      MappedInputManager::Button::Power};
  std::atomic<uint32_t> paintEpoch{0};
  uint32_t observedEpoch = 0;
  bool waitingForNeutral = true;
  bool touchArmed = false;
  uint16_t buttonsArmed = 0;
};
