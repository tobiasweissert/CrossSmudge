#pragma once
#include <atomic>
#include <cstring>
#include "activities/Activity.h"
#include "components/UITheme.h"
#include "fontIds.h"

// One exact ASCII domain per screen, no ellipsis. Allow is unavailable unless every character was rendered.
class LuaConsentActivity : public Activity {
 public:
  LuaConsentActivity(GfxRenderer& r, MappedInputManager& input, const char* domain, const char* heading = tr(STR_LUA_NETWORK_ALLOW),
                     const char* allowLabel = tr(STR_LUA_NETWORK_ALLOW_BUTTON))
      : Activity("LuaNetworkPermission",r,input), domain_(domain), heading_(heading), allowLabel_(allowLabel) {}
  void onEnter() override { Activity::onEnter(); requestUpdate(); }
  void loop() override {
    if(mappedInput.wasReleased(MappedInputManager::Button::Back)){complete(false);return;}
    int tapped=-1;
    if(mappedInput.wasItemTapped(tapped)){complete(tapped==1 && fits_.load());return;}
    if(mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Right) ||
       mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      selected_=fits_.load()?1-selected_:0;requestUpdate();
    }
    if(mappedInput.wasReleased(MappedInputManager::Button::Confirm))complete(selected_==1 && fits_.load());
  }
  void render(RenderLock&&) override {
    fits_.store(false);
    renderer.clearScreen();
    int top,right,bottom,left;renderer.getOrientedViewableTRBL(&top,&right,&bottom,&left);
    const auto& m=UITheme::getInstance().getMetrics();
    const int width=renderer.getScreenWidth()-left-right;
    const int height=renderer.getScreenHeight()-top-bottom;
    const int menuY=top+height*2/3;
    GUI.drawHeader(renderer,Rect{left,top,width,m.headerHeight},heading_);
    int y=top+m.headerHeight+m.verticalSpacing;
    const int lineHeight=renderer.getLineHeight(UI_12_FONT_ID);
    const int x=left+m.verticalSpacing;
    const int textWidth=width-2*m.verticalSpacing;
    size_t offset=0;
    while(offset<domain_.size() && y+lineHeight<=menuY-m.verticalSpacing) {
      char line[128]{};size_t count=0;
      while(offset+count<domain_.size() && count<sizeof(line)-1) {
        line[count]=domain_[offset+count];line[count+1]=0;
        if(renderer.getTextWidth(UI_12_FONT_ID,line)>textWidth){line[count]=0;break;}
        ++count;
      }
      if(!count)break;
      renderer.drawText(UI_12_FONT_ID,x,y,line);offset+=count;y+=lineHeight;
    }
    const bool fits=offset==domain_.size();
    GUI.drawButtonMenu(renderer,Rect{left,menuY,width,top+height-menuY-m.buttonHintsHeight},fits?2:1,selected_,
      [this](int i){return i?allowLabel_:tr(STR_CANCEL);},[](int){return UIIcon::None;});
    const auto labels=mappedInput.mapLabels(tr(STR_BACK),tr(STR_SELECT),tr(STR_DIR_UP),tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer,labels.btn1,labels.btn2,labels.btn3,labels.btn4);
    renderer.displayBuffer();
    fits_.store(fits);
  }
 private:
  std::string domain_;
  const char* heading_;
  const char* allowLabel_;
  std::atomic<bool> fits_{false};
  std::atomic<int> selected_{0};
  void complete(bool yes){ActivityResult result;result.isCancelled=!yes;setResult(std::move(result));finish();}
};
