#include <cassert>
#include <cstdio>
#if __has_include("activities/settings/FindMySettingsActivity.h")
#include "activities/settings/FindMySettingsActivity.h"
#include "findmy/Firmware.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>
namespace crosscub::findmy::firmware {
int fresh=0,existing=0,stops=0;
bool ownerEnableFresh(){++fresh;return true;}bool ownerEnableExisting(){++existing;return true;}
bool beforeAirDrop(){++stops;return true;}const char* statusText(){return "Off";}
}
int main(){
 GfxRenderer renderer;MappedInputManager input;
 FindMySettingsActivity activity(renderer,input);
 input.y=145;input.tap=true;activity.loop();assert(crosscub::findmy::firmware::fresh==0);
 input.tap=true;activity.loop();assert(crosscub::findmy::firmware::fresh==1);
 input.y=205;input.tap=true;activity.loop();assert(crosscub::findmy::firmware::existing==0);
 input.y=325;input.tap=true;activity.loop();assert(activity.finished);
 puts("PASS actual experimental settings controls: two-step owner confirmation and Back exit");
}
#else
int main(){assert(false && "experimental owner settings activity missing");}
#endif
