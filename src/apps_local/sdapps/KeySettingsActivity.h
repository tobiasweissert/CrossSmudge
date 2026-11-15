#pragma once
#if defined(FREEINK_DEVICE_STICKY)
#include "activities/UiListActivity.h"
namespace sdapps {
class KeySettingsActivity final : public UiListActivity {
 public:
  KeySettingsActivity(GfxRenderer& r, MappedInputManager& i) : UiListActivity("OpenAiKeySettings", r, i) {}
  void onEnter() override;
 private:
  void buildScreen(UiScreen&) override;
  int listCount() const override { return 3; }
  void activateIndex(int) override;
  const char* headerTitle() const override { return "OpenAI API key"; }
  void refresh(const char* message);
  freeink::ui::ListItem rows[3]{};
  bool configured = false;
  const char* status = "";
};
}
#endif
