#pragma once
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL
#include "activities/Activity.h"
class FindMySettingsActivity final : public Activity {
 public:
 FindMySettingsActivity(GfxRenderer& r,MappedInputManager& m):Activity("FindMyExperimental",r,m){}
 void loop() override;
 void render(RenderLock&&) override;
 private:
 int selected_=0,confirm_=-1;
 const char* lastStatus_=nullptr;
};
#endif
