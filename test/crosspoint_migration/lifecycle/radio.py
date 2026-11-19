from pathlib import Path
import subprocess,json
from run import ROOT,body

def run(evidence,sanitize=False):
 evidence.mkdir(parents=True,exist_ok=True)
 s=(ROOT/'src/main.cpp').read_text()
 pre=r'''
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <string>
#include <optional>
#include "BlePageTurner.h"
#include "util/ScreenshotInfo.h"
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
struct RenderLock{enum class Mode{Try};RenderLock(){}RenderLock(Mode){}bool ownsLock(){return true;}};
struct FontCache{int freed=0;void releaseSdFontCaches(){++freed;}}fontCacheManager;
struct HalMemory{struct Heap{size_t freeBytes=100,largestBlockBytes=90;};static Heap getInternalHeap(){return {};}};
struct HalPowerManager {struct Lock{Lock(){}~Lock(){}};static constexpr int IDLE_POWER_SAVING_MS=50;};
struct Power{int sleeps=0,battery=80;void setPowerSaving(bool){}int getBatteryPercentage(){return battery;}void startDeepSleep(int){++sleeps;}}powerManager;
struct Clock{bool isAvailable(){return false;}}halClock;
struct Renderer{}renderer;int gpio=0,restarts=0,delays=0,prepares=0,emits=0,drains=0,joins=0,sdShutdown=0;
unsigned long now=0;unsigned long millis(){return now;}void delay(int n){now+=n;++delays;}
void silentRestartToReader(){++restarts;}
struct Manager{bool blocked=false,exclusive=false,remote=true;int sleeps=0;
bool blocksGlobalInput(){return blocked;}bool requiresExclusiveStorageLoop(){return exclusive;}bool isRemoteReaderContext(){return remote;}
bool remoteTurn(bool,bool){return remote;}bool remoteShortcut(bool){return remote;}bool remoteClick(int){return remote;}
void prepareForSleep(){++prepares;}ScreenshotInfo getScreenshotInfo(){return {};}
bool isReaderActivity(){return true;}std::string getCurrentBookPath(){return "book";}void goToSleep(bool){++sleeps;}}activityManager;
namespace RemoteButton {constexpr int Confirm=1,Back=2;}
namespace trustedtime {int notes=0,syncs=0;void note(){++notes;}void startSync(){++syncs;}}
namespace pluginevents {struct Var{const char*key;const char*value;};enum class Event{SleepEnter};bool connect=false;
void emit(Event,Var*,size_t){++emits;}void drain(Renderer*){++drains;}bool wantsConnectAny(){return connect;}}
constexpr int WL_CONNECTED=1,WIFI_MODE_NULL=0,WIFI_STA=1,WIFI_OFF=0;
struct Wifi{int modeValue=0;bool connected=false;int status(){return connected?WL_CONNECTED:0;}int getMode(){return modeValue;}
void mode(int n){modeValue=n;}void begin(const char*,const char*){++joins;}void disconnect(bool){connected=false;}}WiFi;
struct Credential{std::string ssid="s",password="p";};struct Credentials{bool available=false;void loadFromFile(){}std::string getLastConnectedSsid(){return "s";}
std::optional<Credential>findCredential(std::string){return available?std::optional<Credential>(Credential{}):std::nullopt;}}WIFI_STORE;
struct GUIType{template<class...A>void drawPopup(A&&...){}}GUI;constexpr int STR_LOADING_POPUP=0;const char*tr(int){return "loading";}
struct CrossPointSettings{enum class SLEEP_SCREEN_MODE{QUICK_RESUME,OTHER};enum class QUICK_RESUME_SLEEP_SCREEN{QUICK_RESUME_AFTER_TIMEOUT,OTHER};};
struct Settings{CrossPointSettings::SLEEP_SCREEN_MODE sleepScreen=CrossPointSettings::SLEEP_SCREEN_MODE::OTHER;
CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN quickResumeSleepScreen=CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::OTHER;int autoBackupStats=0;}SETTINGS;
struct State{bool lastSleepFromReader=false,showBootScreen=true;std::string openEpubPath;void saveToFile(){}}APP_STATE;
bool deepSleepInProgress=false;constexpr int POST_SLEEP_SCREEN_SETTLE_MS=1;constexpr const char*SLEEP_FRAME_FILE="frame";
void saveSleepFrameBuffer(){}struct ReadingStatsDateTime{};bool getCurrentLocalReadingStatsDateTime(ReadingStatsDateTime&){return false;}bool backupGlobalStats(bool){return true;}
struct BatteryDiagnosticLog{enum class Event{Sleep};static void record(Event,const char*){}};
struct BoardConfig{struct Board{const char*name="board";};static Board ACTIVE;};BoardConfig::Board BoardConfig::ACTIVE;
struct StorageType{bool exists(const char*){return false;}void remove(const char*){}void shutdown(){++sdShutdown;}}Storage;
void putTiltSensorToSleepForDeepSleep(){}struct Display{void deepSleep(){}}display;void mirrorWakeShortPressToNvs(){}
namespace bleturner {bool stopped=true;int stops=0;bool beforeSleep(uint32_t){++stops;return stopped;}}
'''
 namespace=s[s.index('namespace pageturner {'):s.index('}  // namespace pageturner')+len('}  // namespace pageturner')]
 test=r'''
int main(){
 bleturner::stopped=false;enterDeepSleep(false);assert(powerManager.sleeps==0);assert(!deepSleepInProgress);assert(prepares==0);assert(sdShutdown==0);
 bleturner::stopped=true;enterDeepSleep(false);assert(powerManager.sleeps==1);assert(prepares==1);assert(emits==1);assert(sdShutdown==1);
 // BLE cache-release/host callbacks never own the WiFi or plugin Sync lifetime.
 WiFi.modeValue=WIFI_STA;WiFi.connected=true;int oldMode=WiFi.modeValue;int oldDrains=drains;
 pageturner::releaseCaches();assert(WiFi.modeValue==oldMode);assert(drains==oldDrains);
 activityManager.blocked=true;deliverSleepPluginEvents(true);assert(drains==oldDrains);
 activityManager.blocked=false;deliverSleepPluginEvents(false);assert(drains==oldDrains);
 deliverSleepPluginEvents(true);
#if defined(CROSSCUB_VERIFIED_PLUGIN_TRANSPORT) && CROSSCUB_VERIFIED_PLUGIN_TRANSPORT
 assert(drains==oldDrains+1);
#else
 assert(drains==oldDrains);
#endif
 WiFi.connected=false;pluginevents::connect=true;WIFI_STORE.available=true;now=0;
 deliverSleepPluginEvents(true);
#if defined(CROSSCUB_VERIFIED_PLUGIN_TRANSPORT) && CROSSCUB_VERIFIED_PLUGIN_TRANSPORT
 assert(joins==1);assert(now>=10000&&now<10200);
#else
 assert(joins==0);assert(now==0);
#endif
 pageturner::radioStorageBusy=true;assert(!pageturner::yieldForRadio());pageturner::restartIntoBook();assert(restarts==0);
 pageturner::radioStorageBusy=false;activityManager.remote=false;pageturner::restartIntoBook();assert(restarts==0);
 activityManager.remote=true;pageturner::restartIntoBook();assert(restarts==1);
 puts("production entrypoint BLE/plugin/sleep bodies PASS");
}
'''
 cpp=evidence/'radio.cpp';cpp.write_text(pre+namespace+'\n'+body(s,'static void deliverSleepPluginEvents(')+'\n'+body(s,'void enterDeepSleep(bool fromTimeout)')+test)
 cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(evidence),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'lib/BlePageTurner/include'),str(cpp),'-o',str(evidence/'radio')]
 sdk='/root/github/crosssmudge/.git/modules/freeink-sdk'
 (evidence/'BleRemoteProfiles.h').write_bytes(subprocess.check_output(['git','--git-dir='+sdk,'show','634a3f8c067a3714a6a7c622015138af096ab5d2:libs/network/BleKeyboardHost/include/BleRemoteProfiles.h'],timeout=30))
 if sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie']
 results=[]
 for name,args in [('compile-radio',cmd),('execute-radio',[str(evidence/'radio')]),('compile-qualified-radio',[*cmd[:1],'-DCROSSCUB_VERIFIED_PLUGIN_TRANSPORT=1',*cmd[1:-1],str(evidence/'radio-qualified')]),('execute-qualified-radio',[str(evidence/'radio-qualified')])]:
  p=subprocess.run(args,capture_output=True,text=True,timeout=60);(evidence/(name+'.log')).write_text(p.stdout+p.stderr);results.append({'name':name,'exit':p.returncode})
  if p.returncode:break
 (evidence/'results.json').write_text(json.dumps(results,indent=2));print(results)
 return int(any(x['exit'] for x in results))
if __name__=='__main__':
 import sys
 raise SystemExit(run(Path(sys.argv[1]),'--sanitize' in sys.argv))
