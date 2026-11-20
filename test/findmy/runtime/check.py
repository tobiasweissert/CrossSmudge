#!/usr/bin/env python3
"""Local routine entrypoint; actual Core + firmware adapter, controlled HAL, no RF."""
from pathlib import Path
import os, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[3]
OUT=Path('/root/.hermes/task-state/crosscub-findmy-implementation/integration');OUT.mkdir(parents=True,exist_ok=True)
label=sys.argv[1] if len(sys.argv)>1 else 'final'
if Path(label).name!=label: raise SystemExit('invalid label')
logs=[]
sanitize=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g1','-no-pie'] if os.environ.get('FINDMY_RUNTIME_SANITIZE')=='1' else []
def run(cmd):
 r=subprocess.run(cmd,capture_output=True,text=True); logs.append('COMMAND '+' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr+f'EXIT {r.returncode}\n');return r.returncode
with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch'),prefix='findmy-integration-') as d:
 d=Path(d)
 (d/'HalClock.h').write_text('struct Clock {bool synced=true;bool hasSynchronizedSystemTime(){return synced;}};inline Clock halClock;\n')
 (d/'esp_timer.h').write_text('#include <cstdint>\ninline int64_t fakeUs=0;inline int64_t esp_timer_get_time(){return fakeUs;}\n')
 (d/'esp_random.h').write_text('#include <cstddef>\ninline void esp_fill_random(void* p,size_t n){auto b=(unsigned char*)p;while(n--)*b++=7;}\n')
 (d/'HalStorage.h').write_text('''#include <string>
#include <cstring>
inline std::string importJson;inline int sizeCalls=0;inline bool changingSize=false;
struct HalFile {bool isDirectory(){return false;} operator bool()const{return true;} size_t size(){++sizeCalls;return changingSize&&sizeCalls>=3?513:importJson.size();} int read(void* p,size_t n){if(n>512)std::abort();memcpy(p,importJson.data(),n);return n;}bool close(){return true;}};
struct StorageType{HalFile open(const char*){return {};}bool ready(){return true;}};inline StorageType Storage;
''')
 (d/'nvs.h').write_text('''#pragma once
#include <map>
#include <vector>
#include <string>
#include <cstring>
using nvs_handle_t=int;using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,NVS_READWRITE=1;
inline std::map<std::string,std::vector<unsigned char>> nv;inline bool failCommit=false,corruptCheckpointRead=false;inline int64_t delayCommitUs=0;inline int commitCount=0,failCommitAt=0;
inline int nvs_open(const char*,int,nvs_handle_t* h){*h=1;return 0;}inline void nvs_close(nvs_handle_t){}
inline int nvs_get_blob(nvs_handle_t,const char* k,void* p,size_t* n){auto i=nv.find(k);if(i==nv.end())return 1;if(*n<i->second.size())return 2;*n=i->second.size();memcpy(p,i->second.data(),*n);if(corruptCheckpointRead&&std::string(k)=="checkpoint"&&*n)((unsigned char*)p)[0]^=1;return 0;}
inline int nvs_set_blob(nvs_handle_t,const char* k,const void* p,size_t n){auto b=(const unsigned char*)p;nv[k]={b,b+n};return 0;}inline int nvs_commit(nvs_handle_t){++commitCount;fakeUs+=delayCommitUs;delayCommitUs=0;return failCommit||commitCount==failCommitAt?2:0;}
''')
 (d/'activities').mkdir()
 (d/'activities/Activity.h').write_text('''#pragma once
#include <GfxRenderer.h>
#include <MappedInputManager.h>
struct RenderLock{};
struct Activity {GfxRenderer& renderer;MappedInputManager& mappedInput;bool finished=false;Activity(const char*,GfxRenderer& r,MappedInputManager& m):renderer(r),mappedInput(m){}virtual ~Activity()=default;virtual void loop(){}virtual void render(RenderLock&&){}void requestUpdate(){}void finish(){finished=true;}};
''')
 (d/'GfxRenderer.h').write_text('''#pragma once
struct GfxRenderer {int getScreenHeight()const{return 480;}void clearScreen(){}void drawCenteredText(int,int,const char*){}void displayBuffer(){}};
''')
 (d/'MappedInputManager.h').write_text('''#pragma once
struct MappedInputManager {enum class Button{Back,Confirm,Up,Down};int y=0;bool tap=false;bool wasReleased(Button){return false;}bool wasPressed(Button){return false;}bool wasScreenTapped(int& x,int& out){x=10;out=y;bool t=tap;tap=false;return t;}};
''')
 (d/'fontIds.h').write_text('constexpr int UI_10_FONT_ID=1,UI_12_FONT_ID=2;')
 prod=list(map(str,(ROOT/'lib/FindMy').glob('*.cpp')))
 base=['g++','-std=c++17']+sanitize+['-Wall','-Wextra','-Werror','-DFINDMY_CRYPTO_OPENSSL','-I'+str(ROOT/'lib/FindMyRuntime'),'-I'+str(ROOT/'lib/FindMy'),'-I'+str(ROOT/'lib/HalFindMyBle'),'-I'+str(ROOT/'src/findmy'),'-I'+str(d)]
 code=run(['g++','-std=c++17']+sanitize+['-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(ROOT/'test/findmy/runtime/default_off.cpp'),str(ROOT/'src/findmy/Firmware.cpp'),'-o',str(d/'default_off')])
 if code==0:code=run([str(d/'default_off')])
 for name in ['integration','private_state','firmware','ui']:
  cmd=base+['-I'+str(ROOT/'src')]+(['-DCROSSCUB_FINDMY_EXPERIMENTAL=1'] if name in ('firmware','ui') else [])+[str(ROOT/f'test/findmy/runtime/{name}.cpp')]+prod
  if name=='ui' and (ROOT/'src/activities/settings/FindMySettingsActivity.cpp').exists():cmd+=[str(ROOT/'src/activities/settings/FindMySettingsActivity.cpp')]
  if name=='firmware' and (ROOT/'src/findmy/Firmware.cpp').exists():cmd+=[str(ROOT/'src/findmy/Firmware.cpp')]
  cmd+=['-lcrypto','-o',str(d/name)]
  rc=run(cmd)
  if rc==0:rc=run([str(d/name)])
  if rc==0 and name=='firmware':
   for mode in ['slowcommit','no-time','nvs-fail','old-intent','corrupt-state','bad-import','existing-missing','uncertain','lost-time','size-race','checkpoint-fail','checkpoint-readback-fail','handoff-before-enable']:
    r=run([str(d/name),mode]);rc=rc or r
  code=code or rc
(OUT/(label+'.log')).write_text(''.join(logs));print(''.join(logs));sys.exit(code)
