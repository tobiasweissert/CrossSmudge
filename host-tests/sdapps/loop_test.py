#!/usr/bin/env python3
"""Compile exact production loop against deterministic hardware/worker adapters."""
from pathlib import Path
import argparse,subprocess,os,json,hashlib
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=root/'src/apps_local/sdapps/ModuleActivity.cpp';text=source.read_text();loop=text[text.index('void ModuleActivity::loop()'):text.index('int ModuleActivity::presentV2(')]
fixture=r'''
#include "PresentationLayout.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
namespace fui=freeink::ui;
namespace presentation=sdapps::presentation;
struct RenderLock{};
struct MappedInputManager {
 enum class Button {Confirm,Back,Up,Down,PageForward,PageBack};
 enum class SwipeDir {None,Left,Right,Up,Down};
 bool pressed=false,delayed=false,held=false,released=false,confirm=false,back=false,swallowed=false;int x=0,y=0;SwipeDir swipe=SwipeDir::None;
 bool wasScreenTouchPressed(int& a,int& b)const{a=x;b=y;return pressed;}
 bool wasScreenTouchDown(int& a,int& b)const{a=x;b=y;return delayed;}
 bool isScreenTouchHeld(int& a,int& b)const{a=x;b=y;return held;}
 bool wasScreenTouchReleased()const{return released;}
 bool wasPressed(Button b)const{return b==Button::Confirm&&confirm;}
 bool isPressed(Button b)const{return wasPressed(b);}
 bool wasReleased(Button b)const{return b==Button::Back&&back;}
 void swallowCurrentTouch(){swallowed=true;}
 SwipeDir wasSwipe()const{return swipe;}
};
namespace shelf {void leave(int,MappedInputManager&) {}}
struct Runtime {
 enum class Child{None,Edit,Wifi,Key};enum class Job{Request,Capture};
 struct Model{CpViewKind kind=CP_LIST;unsigned count=0;uint32_t flags[9]{};int ids[9]{};} data;
 Model* model=&data;presentation::Model* design=nullptr;
 bool hold=false,touchHold=false,busy=false,pending=false,cancelled=false,bodyGesture=false;
 int holdId=0,jobId=0,resultStatus=0;void* task=nullptr;std::atomic<bool> done{false};
 struct Buffer{void reset(){}}wav;
 std::string key,result,location;Child child=Child::None;Job job=Job::Capture;
 struct Nav{int selected=0;int inputPageRows(){return 1;}void requestSelection(int i){selected=i;}}nav;
 unsigned visibleLines=1,topLine=0,lineCount=1;
};
class ModuleActivity {
 public:
 using Runtime=::Runtime;Runtime* runtime;MappedInputManager mappedInput;int renderer=0;bool leaving=false,started=true,ready=true;int jobs=0,updates=0;
 std::vector<CpEventKind> events;
 struct App{
  bool hitPublished(int x,int y,int,fui::Interaction& hit){
   if(x>=24&&x<456&&y>=100&&y<600){hit.value=int(presentation::Control::Body);return true;}
   if(x>=400&&x<464&&y>=720&&y<780){hit.value=int(presentation::Control::ComposeAction);return true;}
   return false;
  }
  void clearTapFlash(){}
 }app;
 struct Route{fui::ActionEvent event{};fui::InputSnapshot snap{};explicit operator bool()const{return bool(event);}};
 Route routeTouch(const MappedInputManager&){return {};}
 bool routingReady()const{return ready;}
 void resetUi(){ready=false;}
 void requestUpdate(){++updates;}
 void emit(CpEventKind k,int32_t=0,int32_t=0,const char*=nullptr,size_t=0,const char*=nullptr){events.push_back(k);}
 void activate(int){}void openChild(){}void startJob(){++jobs;runtime->pending=false;}
 void loop();
};
LOOP
int main(){
 Runtime r;auto d=std::make_unique<presentation::Model>();r.design=d.get();d->valid=true;d->pageCount=3;d->binding.hold=5;
 ModuleActivity a{&r};
 // Immediate contact, no delayed tap selection: a fast body swipe works.
 a.mappedInput.pressed=true;a.mappedInput.x=80;a.mappedInput.y=200;a.loop();assert(r.bodyGesture);
 a.mappedInput.pressed=false;a.mappedInput.swipe=MappedInputManager::SwipeDir::Up;a.loop();assert(d->page==1&&!r.bodyGesture);
 // A subsequent fast composer swipe must not inherit the body origin.
 a.ready=true;a.mappedInput={};a.mappedInput.pressed=true;a.mappedInput.x=100;a.mappedInput.y=750;a.loop();assert(!r.bodyGesture);
 a.mappedInput.pressed=false;a.mappedInput.swipe=MappedInputManager::SwipeDir::Up;a.loop();assert(d->page==1);
 // A rejected capture followed by dragging out stays in the conversation.
 a.mappedInput={};a.mappedInput.held=true;a.mappedInput.x=80;a.mappedInput.y=200;r.hold=true;r.touchHold=true;r.busy=false;a.events.clear();a.loop();assert(!r.hold&&a.events.empty()&&a.mappedInput.swallowed);
 // An accepted capture drag-out cancels, suppressing its trailing touch.
 r.hold=true;r.busy=true;r.jobId=r.holdId=7;r.job=Runtime::Job::Capture;a.loop();assert(a.events.back()==CP_BACK);
 // Release is emitted before a pending worker may start.
 a.events.clear();r.hold=true;r.pending=true;a.mappedInput.held=false;a.loop();assert(a.events.back()==CP_HOLD_END&&a.jobs==0);
}
'''.replace('const char*=','const char* =').replace('LOOP',loop)
cpp=a.output/'production_loop.cpp';cpp.write_text(fixture);exe=a.output/'production-loop';flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
commands=[['g++','-std=c++20',*flags,'-I'+str(root/'src/apps_local/sdapps'),'-I'+str(root/'freeink-sdk/libs/ui/FreeInkUI/include'),str(cpp),'-o',str(exe)],[str(exe)]]
runs=[]
for cmd in commands:
 r=subprocess.run(cmd,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});runs.append({'command':cmd,'exit':r.returncode,'output':r.stdout+r.stderr})
 if r.returncode:print(r.stdout+r.stderr);break
(a.output/'result.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'runs':runs,'limit':'Exact production loop; hardware input/worker/locking/routing adapters. Not device execution.'},indent=2))
print('production loop',runs[-1]['exit']);raise SystemExit(runs[-1]['exit'])
