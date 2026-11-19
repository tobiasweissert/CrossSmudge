#pragma once
#include <functional>
#include <string>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

class ConfirmationActivity : public Activity {
 private:
  std::string popupTitle;
  std::string cancelLabel;
  std::string confirmLabel;
  OptionPopup confirmPopup;
  bool ignoreConfirmRelease = false;
  bool overrideDisabledReaderTouchscreen = false;

 public:
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, bool ignoreInitialConfirmRelease = false,
                       bool overrideDisabledReaderTouchscreen = false);
  // Explicit label overloads must not convert modern const-char* labels into
  // the legacy consent/reader-touch Boolean arguments.
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, const std::string& cancelLabel, const std::string& confirmLabel);
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, const char* cancelLabel, const char* confirmLabel = nullptr)
      : ConfirmationActivity(renderer, mappedInput, heading, body, std::string(cancelLabel ? cancelLabel : ""),
                             std::string(confirmLabel ? confirmLabel : "")) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
};
