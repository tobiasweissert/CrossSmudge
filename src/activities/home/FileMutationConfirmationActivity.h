#pragma once

#include "activities/util/ConfirmationActivity.h"
#include "FileMutationInputGate.h"

// Only Files mutations opt in. Other confirmations retain their existing input
// behavior; the ordinary popup still paints Cancel as the initial selection.
class FileMutationConfirmationActivity final : public ConfirmationActivity {
 public:
  using ConfirmationActivity::ConfirmationActivity;
  void render(RenderLock&& lock) override {
    inputGate.beginPaint();
    ConfirmationActivity::render(std::move(lock));
    inputGate.endPaint();
  }
  void loop() override {
    if (!inputGate.allowInput(mappedInput)) return;
    ConfirmationActivity::loop();
  }
 private:
  FileMutationInputGate inputGate;
};
