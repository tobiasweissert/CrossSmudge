#pragma once
#include "DictationDevice.h"
#include "activities/Activity.h"
namespace dictation {
class DictationActivity final : public Activity {
 public:
  DictationActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Notes dictation", r, i) {}
  void render(RenderLock&&) override;
  void loop() override;
  bool handleHomeGesture() override {
    leaving = true;
    job.cancelled = true;
    return false;
  }
  void prepareForSleep() override {
    leaving = true;
    job.drain();
    releaseRadio();
  }
  void onExit() override;
  void onInputLockChanged(bool locked) override {
    if (locked) revoke();
  }
  void onFrontlightPanelOpened() override { revoke(); }
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  Job job;
  bool ownsRadio = false;
  void releaseRadio();
  std::atomic<int> painted{-1};
  std::atomic<unsigned> paintGeneration{0};
  unsigned seenGeneration = 0;
  int action() const;
  void revoke() {
    revoked = true;
    job.drain();
  }
  bool leaving = false, revoked = false, neutral = false;
  int seen = -1, armed = -1;
  bool touchArm = false;
};
}  // namespace dictation
