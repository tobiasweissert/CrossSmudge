#!/usr/bin/env python3
"""Production Sync UI additions + real HTTP-header codec, credential-store double."""
from pathlib import Path
import argparse, os, subprocess, tempfile, json
R=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
t=(R/'src/activities/settings/KOReaderSettingsActivity.cpp').read_text();start=t.index('  } else if (selectedIndex == 8)');end=t.index('\n}',start)+2;body=t[start:end].replace('  } else if (selectedIndex == 8)','  if (selectedIndex == 8)',1)
s=r'''
#include <cassert>
#include <array>
#include <memory>
#include <string>
#include <variant>
#include <functional>
#include <cstdio>
#include "HttpHeader.h"
enum class KOReaderServerType {CROSSPOINT,KOSYNC,OTHER};
struct Store {KOReaderServerType profile=KOReaderServerType::CROSSPOINT;std::array<HttpHeader,2> headers;bool saveOk=true;auto getServerType(){return profile;}void setServerType(KOReaderServerType p){profile=p;}bool saveToFile(){return saveOk;}const auto& getCustomHeaders(){return headers;}void setCustomHeader(size_t slot,const std::string& n,const std::string& v){headers[slot]={n,v};}} KOREADER_STORE;
struct KeyboardResult {std::string text;};struct ActivityResult {bool isCancelled=false;std::variant<KeyboardResult> data;};
enum class InputType {Text};struct GfxRenderer{};struct MappedInputManager{};struct KeyboardEntryActivity {std::string initial;KeyboardEntryActivity(GfxRenderer&,MappedInputManager&,const char*,const std::string& s,int,InputType):initial(s){}};
int menuNames[11]={};struct {const char* get(int){return "Header";}} I18N;
struct KOReaderSettingsActivity {size_t selectedIndex=8;GfxRenderer renderer;MappedInputManager mappedInput;std::function<void(const ActivityResult&)> callback;std::unique_ptr<KeyboardEntryActivity> keyboard;void requestUpdate(){}void startActivityForResult(std::unique_ptr<KeyboardEntryActivity> k,std::function<void(const ActivityResult&)> cb){keyboard=std::move(k);callback=cb;}void modernSelection();};
void KOReaderSettingsActivity::modernSelection(){
'''+body+r'''
int main(){KOReaderSettingsActivity a;a.modernSelection();assert(KOREADER_STORE.profile==KOReaderServerType::KOSYNC);a.modernSelection();assert(KOREADER_STORE.profile==KOReaderServerType::OTHER);a.modernSelection();assert(KOREADER_STORE.profile==KOReaderServerType::CROSSPOINT);KOREADER_STORE.saveOk=false;a.modernSelection();assert(KOREADER_STORE.profile==KOReaderServerType::CROSSPOINT);KOREADER_STORE.saveOk=true;for(size_t i:{0,1}){a.selectedIndex=9+i;a.modernSelection();a.callback({false,KeyboardResult{"X-Token: secret"}});assert(KOREADER_STORE.headers[i].name=="X-Token"&&KOREADER_STORE.headers[i].value=="secret");assert(formatHttpHeaderLine(KOREADER_STORE.headers[i],true)=="X-Token: ******");a.modernSelection();assert(a.keyboard->initial=="X-Token: secret");a.callback({true,KeyboardResult{"changed"}});assert(KOREADER_STORE.headers[i].value=="secret");KOREADER_STORE.saveOk=false;a.callback({false,KeyboardResult{"X-Token: changed"}});assert(KOREADER_STORE.headers[i].value=="secret");KOREADER_STORE.saveOk=true;}puts("PASS actual Sync server-profile cycle/save-failure rollback, two custom-header editors/cancel/rollback and real codec secret masking");}
'''
with tempfile.TemporaryDirectory(prefix='routes-sync-',dir=os.environ['TMPDIR']) as d:
 d=Path(d);(d/'sync.cpp').write_text(s);cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g','-I'+str(R/'lib/HttpHeader'),'-I'+str(R/'lib/AsciiText')]
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 cmd += [str(d/'sync.cpp'),str(R/'lib/HttpHeader/HttpHeader.cpp'),'-o',str(d/'sync')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;rc=p.returncode
 if not rc:
  p=subprocess.run([str(d/'sync')],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
 (a.evidence/'sync.log').write_text('COMMAND '+repr(cmd)+'\n'+log+'\nEXIT='+str(rc)+'\n');(a.evidence/'sync-results.json').write_text(json.dumps({'rc':rc,'passes':[l for l in log.splitlines() if l.startswith('PASS ')]},indent=2)+'\n');print(log);raise SystemExit(rc)
