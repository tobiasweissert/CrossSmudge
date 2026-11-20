#include "FindMySettingsActivity.h"
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL
#include <GfxRenderer.h>
#include "MappedInputManager.h"
#include "findmy/Firmware.h"
#include "fontIds.h"
void FindMySettingsActivity::loop(){
 namespace fw=crosscub::findmy::firmware;
 if(lastStatus_!=fw::statusText()){lastStatus_=fw::statusText();requestUpdate();}
 if(mappedInput.wasPressed(MappedInputManager::Button::Back)){finish();return;}
 if(mappedInput.wasPressed(MappedInputManager::Button::Down)){selected_=(selected_+1)%4;confirm_=-1;requestUpdate();}
 if(mappedInput.wasPressed(MappedInputManager::Button::Up)){selected_=(selected_+3)%4;confirm_=-1;requestUpdate();}
 int x=0,y=0;bool activate=mappedInput.wasReleased(MappedInputManager::Button::Confirm);
 if(mappedInput.wasScreenTapped(x,y)&&y>=120&&y<360){selected_=(y-120)/60;activate=true;}
 if(!activate)return;
 if(selected_==3){finish();return;}
 if(selected_==2){(void)fw::beforeAirDrop();confirm_=-1;requestUpdate();return;}
 if(confirm_!=selected_){confirm_=selected_;requestUpdate();return;}
 if(selected_==0)(void)fw::ownerEnableFresh();else (void)fw::ownerEnableExisting();
 confirm_=-1;requestUpdate();
}
void FindMySettingsActivity::render(RenderLock&&){
 renderer.clearScreen();
 renderer.drawCenteredText(UI_12_FONT_ID,20,"Find My - experimental");
 renderer.drawCenteredText(UI_10_FONT_ID,55,"Not certified; pairing is not proven here.");
 renderer.drawCenteredText(UI_10_FONT_ID,85,"Private SD import; NTP sync required this boot.");
 const char* labels[]={"Fresh provision + enable (one-time)","Enable existing checkpoint","Stop (reboot to enable again)","Back"};
 for(int i=0;i<4;++i){
  const char* label=confirm_==i?"Confirm owner consent: select again":labels[i];
  renderer.drawCenteredText(UI_10_FONT_ID,140+i*60,label);
 }
 renderer.drawCenteredText(UI_10_FONT_ID,375,crosscub::findmy::firmware::statusText());
 renderer.drawCenteredText(UI_10_FONT_ID,405,"Use Quick Lock. Power draw unmeasured.");
 renderer.drawCenteredText(UI_10_FONT_ID,435,"Never erase checkpoint/intent to retry.");
 renderer.displayBuffer();
}
#endif
