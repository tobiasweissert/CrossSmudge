#!/usr/bin/env python3
"""Compile actual production transition bodies with bounded hardware doubles.
No network, firmware SDK, or device behavior is simulated as physical evidence.
"""
from pathlib import Path
import argparse, os, re, subprocess
R = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--case', choices=['entry', 'exit', 'startup', 'stack', 'all'], default='all')
a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
s = (R/'src/activities/settings/OtaUpdateActivity.cpp').read_text()
def method(name):
    return re.search(r'void OtaUpdateActivity::'+name+r'\([^\n]*\) \{.*?\n\}', s, re.S).group()
settings = (R/'src/activities/settings/SettingsActivity.cpp').read_text()
action = re.search(r'case SettingAction::CheckForUpdates:(.*?)\n\s*case SettingAction::SdFirmwareUpdate:', settings, re.S).group(1)
main = (R/'src/main.cpp').read_text()
selector = re.search(r'const bool useReaderRenderStack =.*?;', main, re.S).group()
stack_constants = '\n'.join(re.findall(r'constexpr uint32_t (?:NETWORK|READER)_RENDER_TASK_STACK_BYTES = .*?;', main))
stack_argument = re.search(r'activityManager.begin\((useReaderRenderStack.*?)\);', main).group(1)
common = r'''
#include <cassert>
#include <cstdint>
#include <memory>
#include <functional>
#include <utility>
#include <cstdio>
#include "SilentRestart.h"
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
int restarts=0, checks=0, installs=0, releases=0, disconnected=0;
bool allocFail=false, connected=true, modeOk=true, renderOk=true;
constexpr int WL_CONNECTED=1,WIFI_STA=1,WIFI_OFF=0,WIFI_MODE_NULL=0;
struct IPAddress {int a; IPAddress(int a=0,int=0,int=0,int=0):a(a){} bool operator!=(IPAddress b){return a!=b.a;}};
struct Wifi {int modeValue=1; int status(){return connected?WL_CONNECTED:0;} IPAddress localIP(){return IPAddress(connected?1:0);} bool mode(int m){if(!modeOk)return false;modeValue=m;return true;}int getMode(){return modeValue;}bool disconnect(bool){++disconnected;connected=false;return true;} } WiFi;
struct {void restart(){++restarts;}} ESP;
void delay(int){} void silentRestart(){++restarts;}
void silentRestartToNetwork(NetworkBootTarget,uint32_t){++restarts;}
struct GfxRenderer{}; GfxRenderer renderer;
struct MappedInputManager {enum class Button {Confirm,Back};bool confirm=false,back=false,tap=false; bool wasReleased(Button){bool r=confirm;confirm=false;return r;}bool wasPressed(Button){bool r=back;back=false;return r;}bool wasScreenTapped(int&,int&){bool r=tap;tap=false;return r;}} mappedInput;
struct ActivityResult {bool isCancelled=false;};
struct Activity {virtual ~Activity()=default;void onEnter(){} void onExit(){}};
struct WifiSelectionActivity:Activity {WifiSelectionActivity(GfxRenderer&,MappedInputManager&) {}};
template<class T,class... A> std::unique_ptr<T> makeUniqueNoThrow(A&&... args){if(allocFail)return {};return std::make_unique<T>(std::forward<A>(args)...);}
struct {void releaseLoadedFont(GfxRenderer&){}void releaseForNetwork(GfxRenderer&){++releases;}} sdFontSystem;
enum class StrId {STR_UPDATE_FAILED};
struct OtaUpdater {enum OtaUpdaterError {OK,NO_UPDATE,HTTP_ERROR,OOM_ERROR};int checkResult=NO_UPDATE,installResult=OK;bool newer=false;int checkForUpdate(){++checks;return checkResult;}bool isUpdateNewer(){return newer;}int installUpdate(void(*)(void*),void*){++installs;return installResult;}};
StrId failureMessageFor(int){return StrId::STR_UPDATE_FAILED;}
enum class RequestUpdateResult {Rendered,Other};
struct OtaUpdateActivity:Activity {
 enum State {WIFI_SELECTION,CHECKING_FOR_UPDATE,WAITING_CONFIRMATION,UPDATE_IN_PROGRESS,NO_UPDATE,FAILED,FINISHED,SHUTTING_DOWN};
 State state=WIFI_SELECTION;StrId failureMessage=StrId::STR_UPDATE_FAILED;OtaUpdater updater;bool finished=false;std::unique_ptr<Activity> child;std::function<void(const ActivityResult&)> callback;
 OtaUpdateActivity()=default;OtaUpdateActivity(GfxRenderer&,MappedInputManager&){}
 void requestUpdate(bool=false){}RequestUpdateResult requestUpdateAndWait(){return renderOk?RequestUpdateResult::Rendered:RequestUpdateResult::Other;}
 void finish(){finished=true;}void startActivityForResult(std::unique_ptr<Activity>&& a,std::function<void(const ActivityResult&)> cb){child=std::move(a);callback=cb;}
 void onEnter();void onExit();void onWifiSelectionComplete(bool);void runUpdateInstall();void loop();
};
struct RenderLock {RenderLock(OtaUpdateActivity&) {}};
struct TouchHeaderBackButton {static bool wasTapped(MappedInputManager&,GfxRenderer&){return false;}};
struct TouchActionButtons {static int indexAt(int,int,int){return -1;}};
int getOtaActionLayout(GfxRenderer&){return 0;}
ACTIVE_WIFI_HELPER
std::unique_ptr<Activity> launched;
void requestUpdate(bool=false){}
void startActivityForResult(std::unique_ptr<Activity>&& a,int){launched=std::move(a);}
int resultHandler=0;
void settingsAction(){switch(1){case 1:
'''+action+r'''
 break;}}
'''
common = common.replace('ACTIVE_WIFI_HELPER', re.search(r'bool hasActiveWifiConnection\(\) \{[^\n]+\}', s).group())
body='\n'.join(method(n) for n in ['onEnter','onExit','onWifiSelectionComplete','runUpdateInstall','loop'])
tests = {
'entry': r'''settingsAction();assert(restarts==0 && "Settings must not reboot to enter OTA");assert(dynamic_cast<OtaUpdateActivity*>(launched.get()));static_cast<OtaUpdateActivity*>(launched.get())->onEnter();assert(checks==1 && restarts==0); allocFail=true;launched.reset();settingsAction();assert(!launched && restarts==0);''',
'exit': r'''for(int result: {OtaUpdater::NO_UPDATE,OtaUpdater::HTTP_ERROR,OtaUpdater::OOM_ERROR}) {OtaUpdateActivity a; a.updater.checkResult=result;a.onWifiSelectionComplete(true);mappedInput.back=true;a.loop();assert(a.finished);a.onExit();assert(restarts==0 && "Backing out must not reboot");assert(WiFi.getMode()==WIFI_OFF);WiFi.modeValue=1;} OtaUpdateActivity cancel;cancel.updater.newer=true;cancel.onWifiSelectionComplete(true);assert(installs==0);mappedInput.back=true;cancel.loop();assert(cancel.finished);cancel.onExit();assert(restarts==0);for(int result:{OtaUpdater::HTTP_ERROR,OtaUpdater::OOM_ERROR,OtaUpdater::OK}){OtaUpdateActivity a;a.updater.newer=true;a.onWifiSelectionComplete(true);assert(a.state==OtaUpdateActivity::WAITING_CONFIRMATION);a.updater.installResult=result;mappedInput.confirm=true;a.loop();a.loop();if(result==OtaUpdater::OK)assert(restarts==1);else {assert(a.state==OtaUpdateActivity::FAILED);assert(restarts==0);}}''',
'startup': r'''connected=false;OtaUpdateActivity a;modeOk=false;a.onEnter();assert(a.state==OtaUpdateActivity::FAILED && !a.child && checks==0);assert(releases==1);modeOk=true;allocFail=true;OtaUpdateActivity b;b.onEnter();assert(b.state==OtaUpdateActivity::FAILED && !b.child);allocFail=false;OtaUpdateActivity c;c.onEnter();assert(c.child);c.callback(ActivityResult{true});assert(c.finished && checks==0);OtaUpdateActivity d;d.onEnter();d.callback(ActivityResult{false});assert(d.finished && checks==0);connected=true;OtaUpdateActivity e;e.onEnter();assert(checks==1);renderOk=false;OtaUpdateActivity f;f.onEnter();assert(checks==1 && f.state==OtaUpdateActivity::FAILED);assert(restarts==0);''',
'stack': (stack_constants+r'''for(bool s3:{false,true})for(bool network:{false,true})for(auto target:{NetworkBootTarget::OTA,NetworkBootTarget::KOREADER_SYNC,NetworkBootTarget::OPDS,NetworkBootTarget::FILE_TRANSFER,NetworkBootTarget::MANAGE_FONTS,NetworkBootTarget::KOREADER_AUTH}) {bool FREEINK_MCU_S3=s3;auto snapshotTarget=static_cast<unsigned>(target);bool isNetworkResume=network;SELECTOR bool expected=!network || (s3 && target!=NetworkBootTarget::KOREADER_AUTH);assert(useReaderRenderStack==expected);assert((STACK_ARGUMENT)==(expected?16384u:8192u));}''').replace('SELECTOR',selector).replace('STACK_ARGUMENT',stack_argument),
}
for name, test in tests.items():
    if a.case not in ('all',name): continue
    source=a.output/(name+'.cpp');exe=a.output/name
    source.write_text(common+body+'\nint main(){'+test+'\nputs("PASS '+name+'");}\n')
    cmd=['g++','-std=c++17','-I'+str(R/'src'),'-Wall','-Wextra','-Werror','-g',str(source),'-o',str(exe)]
    if a.sanitize:cmd += ['-fsanitize=undefined','-fno-sanitize-recover=all']
    subprocess.run(cmd,check=True,timeout=60)
    subprocess.run([str(exe)],check=True,timeout=10,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
