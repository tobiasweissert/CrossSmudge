from pathlib import Path
import subprocess,json
from run import ROOT,body

def run(evidence,sanitize=False):
 evidence.mkdir(parents=True,exist_ok=True)
 source=(ROOT/'src/activities/ActivityManager.cpp').read_text()
 pre=r'''
#include <cassert>
#include <atomic>
#include <array>
#include <memory>
#include <algorithm>
#include <string>
#include <cstdio>
#include "util/EdgeSlide.h"
#define CROSSINK_APP_CAP_AIRDROP 1
#define LOG_ERR(...) ((void)0)
struct GfxRenderer{int getScreenWidth(){return 600;}int getScreenHeight(){return 800;}};
struct MappedInputManager{enum class SwipeDir{Up};struct CompletedSwipe{};struct CompletedRotation{};bool multi=false;bool wasCompletedMultiTouchSwipe(CompletedSwipe&){return multi;}bool wasCompletedMultiTouchRotation(CompletedRotation&){return false;}bool completed=true,suppressed=false;int sx=2,sy=500,ex=2,ey=300;
bool wasSwipeWithPoints(SwipeDir&,int&a,int&b,int&c,int&d){a=sx;b=sy;c=ex;d=ey;return completed;}
void suppressCurrentTouchContact(){suppressed=true;}};
struct Activity{bool reader=true,blocked=false,exclusive=false;int menus=0;
bool isReaderActivity(){return reader;}bool blocksGlobalInput(){return blocked;}bool requiresExclusiveStorageLoop(){return exclusive;}
bool openReaderSettingsMenu(){++menus;return true;}void handleTwoFingerSwipeAction(int){++menus;}virtual ~Activity()=default;};
int allocations=0;struct AirDropReceiveActivity:Activity{AirDropReceiveActivity(GfxRenderer&,MappedInputManager&,const std::string&){++allocations;}};
template<class T,class...A>std::unique_ptr<T>makeUniqueNoThrow(A&&...a){return std::make_unique<T>(std::forward<A>(a)...);}
namespace bleturner{bool stopped=false;bool stopNow(int){return stopped;}}
struct Light{int value=50;bool present(){return true;}int brightness(){return value;}bool hasColorTemperature(){return true;}int warmth(){return value;}bool isOn(){return true;}void setWarmth(int n){value=n;}void setBrightness(int n){value=n;}void setOn(bool){}}Frontlight;
struct CrossPointSettings {enum TWO_FINGER_SWIPE_ACTION {TWO_FINGER_SWIPE_NOT_SET,TWO_FINGER_SWIPE_INCREASE_BRIGHTNESS,TWO_FINGER_SWIPE_DECREASE_BRIGHTNESS,TWO_FINGER_SWIPE_INCREASE_WARMTH,TWO_FINGER_SWIPE_DECREASE_WARMTH,TWO_FINGER_SWIPE_NEXT_CHAPTER,TWO_FINGER_SWIPE_PREVIOUS_CHAPTER,TWO_FINGER_SWIPE_INCREASE_FONT_SIZE,TWO_FINGER_SWIPE_DECREASE_FONT_SIZE,TWO_FINGER_SWIPE_ACTION_COUNT};};
struct Settings{uint8_t leftEdgeUp=0,leftEdgeDown=0,rightEdgeUp=0,rightEdgeDown=0;int frontlightBrightness=0,frontlightOn=0,frontlightWarmth=0;}SETTINGS;
struct ActivityManager{
enum class PendingAction{None,Push};PendingAction pendingAction=PendingAction::None;
bool inputLocked=false;uint32_t screenVisit=1;std::atomic<uint32_t>paintedVisit{1};
std::unique_ptr<Activity>currentActivity=std::make_unique<Activity>();MappedInputManager mappedInput;GfxRenderer renderer;
int saves=0,replacements=0;void persistGlobalSettings(){++saves;}
void replaceActivity(std::unique_ptr<Activity>&& next){++replacements;currentActivity=std::move(next);}
bool handleEdgeSlide();bool goToAirDropReceive(const std::string&);
};
'''
 test=r'''
int main(){
 ActivityManager a;assert(!a.handleEdgeSlide());assert(!a.mappedInput.suppressed);assert(a.saves==0);
 SETTINGS.leftEdgeUp=CrossPointSettings::TWO_FINGER_SWIPE_INCREASE_BRIGHTNESS;assert(a.handleEdgeSlide());assert(Frontlight.value==55);assert(a.saves==1);
 a.mappedInput.multi=true;assert(!a.handleEdgeSlide());a.mappedInput.multi=false;
 a.mappedInput.suppressed=false;a.currentActivity->reader=false;assert(!a.handleEdgeSlide());assert(!a.mappedInput.suppressed);
 a.currentActivity->reader=true;a.currentActivity->blocked=true;assert(!a.handleEdgeSlide());a.currentActivity->blocked=false;
 a.inputLocked=true;assert(!a.handleEdgeSlide());a.inputLocked=false;
 a.paintedVisit=0;assert(!a.handleEdgeSlide());a.paintedVisit=1;
 a.mappedInput.sy=30;assert(!a.handleEdgeSlide());a.mappedInput.sy=500;
 a.mappedInput.ex=300;assert(!a.handleEdgeSlide());a.mappedInput.ex=2;
 SETTINGS.leftEdgeUp=CrossPointSettings::TWO_FINGER_SWIPE_NEXT_CHAPTER;assert(a.handleEdgeSlide());assert(a.currentActivity->menus==1);
 assert(!a.goToAirDropReceive("book"));assert(allocations==0);assert(a.replacements==0);
 bleturner::stopped=true;assert(a.goToAirDropReceive("book"));assert(allocations==1);assert(a.replacements==1);
 puts("production edge dispatch/AirDrop radio admission PASS");
}
'''
 cpp=evidence/'edges.cpp';cpp.write_text(pre+body(source,'bool ActivityManager::handleEdgeSlide()')+body(source,'bool ActivityManager::goToAirDropReceive(')+test)
 cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(cpp),'-o',str(evidence/'edges')]
 if sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie']
 results=[]
 for name,args in [('compile-edges',cmd),('execute-edges',[str(evidence/'edges')])]:
  p=subprocess.run(args,capture_output=True,text=True,timeout=60);(evidence/(name+'.log')).write_text(p.stdout+p.stderr);results.append({'name':name,'exit':p.returncode})
  if p.returncode:break
 (evidence/'results.json').write_text(json.dumps(results,indent=2));print(results)
 return int(any(x['exit'] for x in results))
if __name__=='__main__':
 import sys
 raise SystemExit(run(Path(sys.argv[1]),'--sanitize' in sys.argv))
