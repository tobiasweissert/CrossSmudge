#!/usr/bin/env python3
"""Actual Confirmation TU and new Settings action bodies over host doubles."""
from pathlib import Path
import argparse, os, subprocess, tempfile, json
R=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
def stripped(path):return '\n'.join(l for l in (R/path).read_text().splitlines() if not l.startswith('#include') and not l.startswith('#pragma'))
def method(path,signature):
 t=(R/path).read_text();s=t.index(signature);return t[s:t.index('\n}',s)+2]
s=r'''
#include <cassert>
#include <string>
#include <functional>
#include <utility>
#include <memory>
#include <cstdio>
struct GfxRenderer {};
struct MappedInputManager {enum class Button {Confirm};bool released=false,held=false,overrideTouch=false,outside=false;bool wasReleased(Button){return std::exchange(released,false);}bool isPressed(Button){return held;}void setReaderTouchscreenOverride(bool b){overrideTouch=b;}};
struct ActivityResult {bool isCancelled=false;};
struct Activity {struct RenderLock{};bool finished=false;ActivityResult result;Activity(const char*,GfxRenderer&,MappedInputManager& i):mappedInput(i){}MappedInputManager& mappedInput;virtual ~Activity()=default;virtual void onEnter(){}virtual void onExit(){}virtual void loop(){}virtual void render(RenderLock&&){}virtual bool allowPowerAsConfirmInReaderMode()const{return false;}void requestUpdate(bool=false){}void finish(){finished=true;}void setResult(ActivityResult r){result=r;}};
// Explicit OptionPopup double: proves Confirmation orchestration and overload
// selection only. Parent's conflicted production popup is a separate blocker.
struct OptionPopup {static OptionPopup* last;bool active=true;std::string labels[2];std::function<void(int)> callback;int primary=-1,selected=-1;void show(const char*,const char** options,int n,int initial,std::function<void(int)> cb){assert(n==2);labels[0]=options[0];labels[1]=options[1];selected=initial;callback=cb;last=this;}void setPrimaryOptionIndex(int i){primary=i;}bool processRender(GfxRenderer&,MappedInputManager&){return true;}bool handleInput(MappedInputManager& i,std::function<void()>){if(!active)return false;if(i.outside){active=false;i.outside=false;return true;}if(i.wasReleased(MappedInputManager::Button::Confirm)){active=false;callback(1);}return true;}};OptionPopup* OptionPopup::last=nullptr;
enum class StrId {STR_CANCEL,STR_CONFIRM};struct {const char* get(StrId id){return id==StrId::STR_CANCEL?"Cancel":"Confirm";}} I18N;
'''
# Actual class uses renderer as an inherited member.
s=s.replace('MappedInputManager& mappedInput;virtual','GfxRenderer renderer;MappedInputManager& mappedInput;virtual')
s+=stripped('src/activities/util/ConfirmationActivity.h')+'\n'+stripped('src/activities/util/ConfirmationActivity.cpp')
s+=r'''
void confirmationTests(){GfxRenderer r;MappedInputManager i;ConfirmationActivity custom(r,i,"Heading","Body","Keep","Replace");custom.onEnter();assert(OptionPopup::last->labels[0]=="Keep"&&OptionPopup::last->labels[1]=="Replace");assert(!i.overrideTouch);assert(OptionPopup::last->selected==0&&OptionPopup::last->primary==1);i.outside=true;custom.loop();assert(!custom.finished);custom.loop();assert(custom.finished&&custom.result.isCancelled);ConfirmationActivity legacy(r,i,"H","B",true,true);legacy.onEnter();assert(i.overrideTouch);i.released=true;legacy.loop();assert(!legacy.finished);i.released=true;legacy.loop();assert(legacy.finished&&!legacy.result.isCancelled);legacy.onExit();assert(!i.overrideTouch);ConfirmationActivity one(r,i,"H","B","Keep");one.onEnter();assert(OptionPopup::last->labels[0]=="Keep"&&OptionPopup::last->labels[1]=="Confirm");puts("PASS actual Confirmation constructors/custom-label non-bool routing/legacy initial-release/reader-touch/idle cancel orchestration");}
'''
settings=(R/'src/activities/settings/SettingsActivity.cpp').read_text()
names=['Library','Plugins','Bluetooth','ClockSettings','About']
branches=[]
for name in names:
 start=settings.index('      case SettingAction::'+name+':');end=settings.index('\n      case ',start+1);branches.append(settings[start:end])
s+=r'''
struct {int library=0,plugins=0;void goToLibrary(){++library;}void goToPlugins(){++plugins;}} activityManager;
struct {bool available=true;bool isAvailable(){return available;}} halClock;
struct BleSettingsActivity {BleSettingsActivity(GfxRenderer&,MappedInputManager&){}};struct ClockSettingsActivity {ClockSettingsActivity(GfxRenderer&,MappedInputManager&){}};struct AboutActivity {AboutActivity(GfxRenderer&,MappedInputManager&){}};
bool allocFail=false;template<class T,class...Args>auto makeUniqueNoThrow(Args&&...args){return allocFail?std::unique_ptr<T>{}:std::make_unique<T>(std::forward<Args>(args)...);}
int children=0,updates=0;template<class T>void startActivityForResult(std::unique_ptr<T> child,int){assert(child);++children;}void requestUpdate(){++updates;}
enum class SettingAction {Library,Plugins,Bluetooth,ClockSettings,About};
void dispatch(SettingAction action){GfxRenderer renderer;MappedInputManager mappedInput;int resultHandler=0;switch(action){
'''+ '\n'.join(branches)+r'''
}}
int main(){confirmationTests();dispatch(SettingAction::Library);dispatch(SettingAction::Plugins);assert(activityManager.library==1&&activityManager.plugins==1);dispatch(SettingAction::Bluetooth);dispatch(SettingAction::About);dispatch(SettingAction::ClockSettings);assert(children==3);halClock.available=false;dispatch(SettingAction::ClockSettings);assert(children==3);allocFail=true;for(auto action:{SettingAction::Bluetooth,SettingAction::About,SettingAction::ClockSettings})dispatch(action);assert(children==3&&updates==4);puts("PASS actual Settings Library/plugins/BLE/About/timezone action lifecycle and OOM/RTC gates");}
'''
with tempfile.TemporaryDirectory(prefix='routes-flow-',dir=os.environ['TMPDIR']) as d:
 d=Path(d);(d/'flow.cpp').write_text(s);cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g']
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 cmd += [str(d/'flow.cpp'),'-o',str(d/'flow')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=70);log=p.stdout+p.stderr;rc=p.returncode
 if rc==0:
  p=subprocess.run([str(d/'flow')],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
 (a.evidence/'flow.log').write_text('COMMAND '+repr(cmd)+'\n'+log+'\nEXIT='+str(rc)+'\n');(a.evidence/'flow-results.json').write_text(json.dumps({'rc':rc,'passes':[l for l in log.splitlines() if l.startswith('PASS ')]},indent=2)+'\n');print(log);raise SystemExit(rc)
