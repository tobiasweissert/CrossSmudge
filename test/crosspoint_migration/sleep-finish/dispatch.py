#!/usr/bin/env python3
"""Execute the real onEnter body over synthetic orchestration doubles, not pixels."""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[3]
source_path=(root/'src/activities/boot_sleep/SleepActivity.cpp')
if '--source' in sys.argv:
    source_path=Path(sys.argv[sys.argv.index('--source')+1])
source=source_path.read_text()
start=source.index('void SleepActivity::onEnter()')
end=source.index('\nvoid SleepActivity::renderCustomSleepScreen',start)
body=source[start:end]
# onEnter is the only method extracted, even when helpers are inserted afterwards.
level=0; first=body.index('{')
for i in range(first,len(body)):
    level += (body[i]=='{')-(body[i]=='}')
    if level==0: body=body[:i+1]; break
modes=['DARK','LIGHT','CUSTOM','COVER','BLANK','COVER_CUSTOM','OVERLAY','READING_STATS_SLEEP','MINIMAL_SLEEP','QUICK_RESUME','MINIMAL_STATS_SLEEP','DASHBOARD_SLEEP','TRANSPARENT_CUSTOM','COVER_TRANSPARENT']
settings=(root/'src/CrossPointSettings.h').read_text()
enum_body=settings.split('enum SLEEP_SCREEN_MODE {',1)[1].split('};',1)[0]
methods=['Default','Custom','Cover','ReadingStats','Minimal','MinimalStats','Dashboard','LastScreen','Blank','Overlay','TransparentCustom']
prefix='''#include <string>
#include <cassert>
#include <cstdio>
struct GfxRenderer { enum Orientation {Portrait,Landscape}; int inverted=0; void invertScreen(){++inverted;} bool storeBwBuffer(){return true;} void setOrientation(Orientation){} };
struct Display {bool inverted=false; bool isInverted(){return inverted;} void setInverted(bool v){inverted=v;}} display;
struct CrossPointSettings {enum SLEEP_SCREEN_MODE {MODES}; enum QUICK_RESUME_SLEEP_SCREEN{QUICK_RESUME_AFTER_TIMEOUT=1};};
struct Settings {int sleepScreen=0,quickResumeSleepScreen=0,screenInverted=0;} SETTINGS;
struct State {bool lastSleepFromReader=false;std::string openEpubPath;} APP_STATE;
struct Recent {void ensureLoaded(){}} RECENT_BOOKS;
struct Gui{void drawPopup(GfxRenderer&,const char*){}} GUI;
const int STR_ENTERING_SLEEP=0; const char* tr(int){return "sleep";}
struct Activity{void onEnter(){}};
void releaseSdFontCachesForDecode(GfxRenderer&){}
struct SleepActivity:Activity {GfxRenderer renderer;bool fromTimeout=false,overlayBackgroundBufferStored=false;std::string currentBookPath;GfxRenderer::Orientation sleepPopupOrientation=GfxRenderer::Portrait;int result=-1;
void onEnter(); METHODS
};
'''.replace('MODES',enum_body).replace('METHODS','\n'.join(f'void render{m}SleepScreen(){{result={i};}}' for i,m in enumerate(methods)))
main='''int main(){int count=0; for(int mode=0;mode<14;++mode)for(bool inv:{false,true})for(bool preference:{false,true})for(bool reader:{false,true})for(bool timeout:{false,true}){SleepActivity a;SETTINGS.sleepScreen=mode;SETTINGS.screenInverted=preference;SETTINGS.quickResumeSleepScreen=timeout;display.inverted=inv;APP_STATE.lastSleepFromReader=reader;a.fromTimeout=timeout;a.onEnter();int expected=EXPECTED;assert(a.result==expected);assert(display.inverted==(expected==7&&preference));if((mode==12||mode==13)&&!timeout)assert(a.renderer.inverted==inv);++count;}printf("dispatch: %d synthetic real-body cases PASS\\n",count);}
'''.replace('EXPECTED','timeout||mode==9?7:mode==12?10:mode==13?2:mode==0||mode==1?0:mode==2?1:mode==3?2:mode==4?8:mode==5?(reader?2:1):mode==6?9:mode==7?3:mode==8?4:mode==10?5:6')
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
cpp=out/'dispatch.cpp';cpp.write_text(prefix+body+main)
cmd=['g++','-std=c++17','-O0','-g',str(cpp),'-o',str(out/'dispatch')]
if '--sanitize' in sys.argv:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
subprocess.run(cmd,check=True,timeout=60);subprocess.run([str(out/'dispatch')],check=True,timeout=30)
