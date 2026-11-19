#!/usr/bin/env python3
"""Bounded host tests: extract unmodified complete production method bodies.
Hardware/RTOS/reader objects are doubles. No target compilation claim.
"""
import argparse
import json
import pathlib
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]


def body(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    i = opening + 1
    while depth:
        depth += (text[i] == '{') - (text[i] == '}')
        i += 1
    return text[start:i]


def run(evidence, sanitize=False):
    evidence.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'src/activities/ActivityManager.cpp').read_text()
    names = ['void ActivityManager::renderTaskLoop()', 'bool ActivityManager::isForegroundReader()',
             'bool ActivityManager::isRemoteReaderContext()',
             'bool ActivityManager::isForegroundHomeShown()',
             'bool ActivityManager::remoteClick(',
             'bool ActivityManager::remoteShortcut(',
             'bool ActivityManager::remoteTurn(',
             'RequestUpdateResult ActivityManager::requestUpdateAndWait()',
             'RenderLock::RenderLock(Mode mode)',
             'RenderLock::~RenderLock()', 'void RenderLock::unlock()', 'bool RenderLock::peek()']
    prelude = r'''
#include <atomic>
#include <cassert>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <string>
#include "activities/RenderLock.h"
#include "util/EdgeSlide.h"
#define LOG_ERR(...) ((void)0)
#define taskENTER_CRITICAL(x) ((void)0)
#define taskEXIT_CRITICAL(x) ((void)0)
constexpr int pdTRUE=1, portMAX_DELAY=999, eIncrement=1;
using TaskHandle_t=void*;
void* task=(void*)1; void* holder=nullptr;
int takes=0, gives=0, notifications=0; bool takeOK=true;
void* xTaskGetCurrentTaskHandle(){return task;}
void* xSemaphoreGetMutexHolder(int){return holder;}
int xSemaphoreTake(int,int){++takes; return takeOK;}
void xSemaphoreGive(int){++gives;}
void xTaskNotify(void*,int,int){++notifications;}
bool rendering=false;int renderPolls=0;bool published=false;
void ulTaskNotifyTake(int,int){if(rendering && renderPolls++>0)throw 1;}
struct HalPowerManager{struct Lock{Lock(){}~Lock(){}};};
struct TouchRegistry{static TouchRegistry&getInstance(){static TouchRegistry r;return r;}void setEnabled(bool){}void beginFrame(){published=false;}void publish(){published=true;}};
struct Settings{int screenInverted=0;}SETTINGS;struct Display{void setInverted(bool){}}display;
void duringRender();
enum class RequestUpdateResult {Rendered,Rejected};
enum class RemoteButton {None,Next,Previous,Confirm,Back};
struct Input {enum class Button{Any}; static constexpr size_t BUTTON_COUNT=1; bool contact=false,physical=false;bool isScreenContactActive()const{return contact;}bool isPhysicalPressed(Button)const{return physical;}bool hasTouch(){return true;}bool clicked=false; bool injectRemoteClick(RemoteButton){clicked=true;return true;} bool hasRemoteClick(){return clicked;}};
struct Activity {bool reader=true,remote=true,blocked=false,exclusive=false; std::string name="Home";
 virtual ~Activity()=default;void render(RenderLock&&){duringRender();}
 bool isReaderActivity()const{return reader;} bool acceptsRemoteInput()const{return remote;}
 bool blocksGlobalInput()const{return blocked;} bool requiresExclusiveStorageLoop()const{return exclusive;}};
struct ReaderActivity:Activity {std::atomic<bool> pageRendered{true};bool over=false;int turns=0;
 bool inputOverPage(){return over;}bool remoteShortcut(bool){++turns;return true;}
 bool skipPages(int){++turns;return true;}bool pageTurn(bool){++turns;return true;}};
bool remoteTurnAccepted(bool shown,bool locked,bool over){return shown&&!locked&&!over;}
struct ActivityManager {
 friend class RenderLock;
 enum class PendingAction {None,Push};PendingAction pendingAction=PendingAction::None;
 std::unique_ptr<Activity> currentActivity=std::make_unique<ReaderActivity>();
 int renderingMutex=0,renderStateMux=0;void* renderTaskHandle=(void*)2;void* waitingTaskHandle=nullptr;
 using MappedInputManager=Input;
 Input mappedInput;bool inputLocked=false,radioInputAllowed=true;
 uint32_t screenVisit=1;std::atomic<uint32_t> paintedVisit{1};std::atomic<bool> requestedUpdate{false};std::atomic<bool> restoredActivityNeedsRender{true};
 void renderTaskLoop();
 bool isForegroundReader()const;bool isRemoteReaderContext()const;bool isForegroundHomeShown()const;
 bool remoteClick(RemoteButton);bool remoteShortcut(bool);bool remoteTurn(bool,bool);
 RequestUpdateResult requestUpdateAndWait();void requestUpdate(){requestedUpdate=true;}
} activityManager;
void duringRender(){++activityManager.screenVisit;}
'''
    tests = r'''
int main(){
 auto& a=activityManager;auto* reader=static_cast<ReaderActivity*>(a.currentActivity.get());
 assert(a.isRemoteReaderContext());
 a.mappedInput.physical=true;assert(!a.remoteTurn(true,false));a.mappedInput.physical=false;
 a.mappedInput.contact=true;assert(!a.remoteShortcut(true));a.mappedInput.contact=false;
 a.radioInputAllowed=false;assert(!a.remoteClick(RemoteButton::Confirm));a.radioInputAllowed=true;
 a.currentActivity->blocked=true;assert(!a.isRemoteReaderContext());assert(!a.remoteShortcut(true));
 a.currentActivity->blocked=false;a.currentActivity->exclusive=true;assert(!a.remoteTurn(true,false));
 a.currentActivity->exclusive=false;a.inputLocked=true;assert(!a.remoteClick(RemoteButton::Confirm));
 a.inputLocked=false;a.paintedVisit=0;assert(!a.remoteTurn(true,false));
 a.paintedVisit=1;a.pendingAction=ActivityManager::PendingAction::Push;assert(!a.remoteShortcut(true));
 a.pendingAction=ActivityManager::PendingAction::None;assert(a.remoteTurn(true,false));assert(reader->turns==1);
 holder=task;assert(a.requestUpdateAndWait()==RequestUpdateResult::Rejected);assert(notifications==0);
 holder=nullptr;a.waitingTaskHandle=(void*)3;assert(a.requestUpdateAndWait()==RequestUpdateResult::Rejected);
 a.waitingTaskHandle=nullptr;task=a.renderTaskHandle;assert(a.requestUpdateAndWait()==RequestUpdateResult::Rejected);
 task=(void*)1;assert(a.requestUpdateAndWait()==RequestUpdateResult::Rendered);assert(notifications==1);
 a.renderTaskHandle=nullptr;assert(a.requestUpdateAndWait()==RequestUpdateResult::Rejected);
 takeOK=false;{RenderLock lock(RenderLock::Mode::Try);assert(!lock.ownsLock());lock.unlock();}assert(gives==0);
 takeOK=true;{RenderLock lock;assert(lock.ownsLock());lock.unlock();lock.unlock();}assert(gives==1);
 using D=EdgeSlide::Direction;
 assert(EdgeSlide::directionFor(2,400,3,200,600,800)==D::LeftUp);
 assert(EdgeSlide::directionFor(2,-1,3,200,600,800)==D::None);
 assert(EdgeSlide::directionFor(2,400,300,200,600,800)==D::None);
 a.renderTaskHandle=(void*)2;a.waitingTaskHandle=task;a.screenVisit=7;a.paintedVisit=0;
 rendering=true;try{a.renderTaskLoop();}catch(int){}
 assert(a.paintedVisit==7);assert(a.screenVisit==8);assert(published);assert(!a.restoredActivityNeedsRender);
 assert(a.waitingTaskHandle==nullptr);assert(notifications==2);
 puts("production remote/render/edge bodies PASS");
}
'''
    cpp = evidence / 'production-bodies.cpp'
    results=[]
    try:
        cpp.write_text(prelude+'\n'+'\n'.join(body(source,n) for n in names)+tests)
        cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(cpp),'-o',str(evidence/'bodies')]
        if sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie']
        for name, argv in [('compile',cmd),('execute',[str(evidence/'bodies')])]:
            t=time.monotonic(); p=subprocess.run(argv,capture_output=True,text=True,timeout=60)
            (evidence/(name+'.log')).write_text(p.stdout+p.stderr)
            results.append({'name':name,'exit':p.returncode,'seconds':time.monotonic()-t,'argv':argv})
            if p.returncode:break
    except Exception as exc:
        results.append({'name':'prepare','exit':1,'error':str(exc)})
    (evidence/'results.json').write_text(json.dumps(results,indent=2))
    print(json.dumps(results,indent=2))
    return int(any(r['exit'] for r in results))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--evidence',type=pathlib.Path,required=True);parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
    statuses = {'remote-render': run(args.evidence/'remote-render',args.sanitize)}
    import popup
    import radio
    import edges
    import runtime
    for name,module in [('popup',popup),('radio',radio),('edges',edges),('runtime',runtime)]:
        statuses[name] = module.run(args.evidence/name,args.sanitize)
    (args.evidence/'suite-results.json').write_text(json.dumps(statuses,indent=2))
    print(json.dumps(statuses,indent=2))
    raise SystemExit(int(any(statuses.values())))
