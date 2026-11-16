#pragma once
#include "activities/Activity.h"
#include <HalLuaHttp.h>
class WifiSelectionActivity:public Activity {
 public:
 inline static int starts=0,exits=0;
 bool resultConnected=false;
 WifiSelectionActivity(GfxRenderer&r,MappedInputManager&i,bool=true):Activity("Wifi",r,i){}
 void onEnter()override{++starts;}
 void onExit()override{++exits;if(!resultConnected)HalLuaHttp::disconnectWifi();}
};
