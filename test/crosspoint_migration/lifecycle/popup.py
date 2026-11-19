from pathlib import Path
import subprocess, json
from run import ROOT

def run(evidence,sanitize=False):
 evidence.mkdir(parents=True,exist_ok=True)
 st=evidence/'stubs';st.mkdir(exist_ok=True)
 prelude=r'''
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdint>
struct Rect{int x=0,y=0,width=0,height=0;};
struct EpdFontFamily {enum Style{REGULAR,BOLD};};
constexpr int UI_12_FONT_ID=12,UI_10_FONT_ID=10;
struct Metrics {int optionPopupOptionFontBold=0,optionPopupItemSpacing=4,optionPopupInnerPadding=8,
optionPopupSelectionHPadding=4,optionPopupSelectionVPadding=4,optionPopupTitleGap=4,buttonHintsHeight=30,
optionPopupDialogSideMargin=10,scrollBarWidth=3,scrollBarRightOffset=2;};
struct GfxRenderer {int paints=0;int getScreenWidth()const{return 600;}int getScreenHeight()const{return 800;}
int getLineHeight(int)const{return 20;}int getTextWidth(int,const char*,EpdFontFamily::Style=EpdFontFamily::REGULAR)const{return 120;}
int getSpaceWidth(int)const{return 5;}
std::vector<std::string> wrappedText(int,const char* s,int,int,EpdFontFamily::Style=EpdFontFamily::REGULAR)const{return {s};}
void displayBuffer(){++paints;}};
struct GPIO{bool hasTouch(){return true;}};inline GPIO gpio;
using StrId=int;struct I18n {const char* get(int){return "translated";}};inline I18n I18N;
inline const char* tr(int){return "text";}constexpr int STR_CANCEL=1,STR_SAVE=2,STR_SELECT=3,STR_BACK=4,STR_DIR_UP=5,STR_DIR_DOWN=6;
struct UITheme{static UITheme& getInstance(){static UITheme t;return t;}const Metrics& getMetrics(){static Metrics m;return m;}};
struct Gui{std::string renderedTitle;template<class...A>void drawButtonHints(A&&...){}
void drawOptionPopup(const GfxRenderer&,const char* t,const std::vector<std::string>&,int,bool,const char*,const char*,bool,int,const char*,const char*,const std::vector<bool>&,int){renderedTitle=t;}};inline Gui GUI;
struct Scale{int bodyFontId=12;};inline Scale uiScaleSpec(){return {};}
struct TouchActionButtons {struct Layout{};static constexpr int kDefaultGap=4,kDefaultHeight=40;
struct Actions{Rect buttons[2];};static Actions vertical(Rect,int,int,int){return {};}};
class RenderLock{public:static bool busy;static bool peek(){return busy;}};inline bool RenderLock::busy=false;
struct MappedInputManager{
enum class Button{Confirm,Back,Power,Left,Right,Up,Down,NavNext,NavPrevious};enum class SwipeDir{None,Up,Down};
struct Label{Label(const char*){}};struct Labels{const char* btn1="",*btn2="",*btn3="",*btn4="";};
bool down=false,tap=false,confirm=false,back=false,suppressed=false;int x=0,y=0;GfxRenderer renderer;
bool wasScreenTouchDown(int& a,int& b){a=x;b=y;return down;}bool wasScreenTapped(int& a,int& b){a=x;b=y;return tap;}
SwipeDir wasSwipe(){return SwipeDir::None;}const GfxRenderer& getRenderer(){return renderer;}
void suppressCurrentTouchContact(){suppressed=true;}void suppressNextConfirmRelease(){}void suppressNextPowerRelease(){}void suppressNextBackRelease(){}
bool isPressed(Button){return false;}bool wasPressed(Button b){return b==Button::Confirm?confirm:b==Button::Back?back:false;}
bool wasReleased(Button){return false;}Label withBackArrow(const char* s)const{return s;}template<class...A>Labels mapLabels(A&&...)const{return {};}
};
struct ButtonNavigator{
static int previousPageIndex(int i,int n,int v){return (i+n-v)%n;}static int nextPageIndex(int i,int n,int v){return (i+v)%n;}
static int previousIndex(int i,int n){return (i+n-1)%n;}static int nextIndex(int i,int n){return (i+1)%n;}
template<class F>void onPrevious(F){}template<class F>void onNext(F){}
template<class F>void onPreviousRelease(F){}static int getNextButtons(){return 0;}template<class F>void onRelease(int,F){}template<class F>void onNextContinuous(F){}template<class F>void onPreviousContinuous(F){}
};
'''
 (st/'fake.h').write_text(prelude)
 for p in ['HalGPIO.h','I18n.h','GfxRenderer.h','MappedInputManager.h','components/TouchActionButtons.h','components/UIScale.h','components/UITheme.h','fontIds.h','util/ButtonNavigator.h','activities/RenderLock.h']:
  f=st/p;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#include "fake.h"\n')
 test=r'''
#include <cassert>
#include "components/OptionPopup.h"
int main(){
 OptionPopup p;MappedInputManager input;GfxRenderer renderer;int selected=-1,canceled=0,updates=0;
 auto update=[&]{++updates;};const char* opts[]={"disabled","enabled"};
 p.show("title","subject",opts,2,0,[&](int i){selected=i;});p.setDisabledOptions({true,false});
 // No buttons or old release may act before first publication.
 input.confirm=true;p.handleInput(input,update);assert(p.isActive());assert(selected==-1);input.confirm=false;
 p.render(renderer);assert(GUI.renderedTitle.find("subject")!=std::string::npos);
 p.handleButtons(false,false,true,false,update);assert(selected==1);assert(!p.isActive());
 p.showMessage("caption","message",opts,2,1,[&](int i){selected=i;});p.setCancelCallback([&]{++canceled;});
 p.render(renderer);assert(GUI.renderedTitle.find("message")!=std::string::npos);
 p.handleButtons(false,false,false,true,update);assert(canceled==1);
 p.show("again",opts,2,0,[&](int i){selected=i;});p.render(renderer);selected=-1;
 input.tap=true;input.x=100;input.y=100;p.handleInput(input,update);assert(p.isActive());assert(selected==-1);
 p.dismiss();assert(!p.isActive());
 puts("production OptionPopup header PASS");
}
'''
 cpp=evidence/'popup.cpp';cpp.write_text(test)
 cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(st),'-I'+str(ROOT/'src'),str(cpp),'-o',str(evidence/'popup')]
 if sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie']
 results=[]
 for name,args in [('compile-popup',cmd),('execute-popup',[str(evidence/'popup')])]:
  p=subprocess.run(args,capture_output=True,text=True,timeout=60);(evidence/(name+'.log')).write_text(p.stdout+p.stderr);results.append({'name':name,'exit':p.returncode})
  if p.returncode:break
 (evidence/'results.json').write_text(json.dumps(results,indent=2));print(results)
 return int(any(x['exit'] for x in results))
if __name__=='__main__':
 import sys
 raise SystemExit(run(Path(sys.argv[1]),'--sanitize' in sys.argv))
