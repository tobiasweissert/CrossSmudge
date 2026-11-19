from pathlib import Path
import subprocess,json
from run import ROOT

def run(evidence,sanitize=False):
 evidence.mkdir(parents=True,exist_ok=True)
 sdk='/root/github/crosssmudge/.git/modules/freeink-sdk'
 (evidence/'BleRemoteProfiles.h').write_bytes(subprocess.check_output(['git','--git-dir='+sdk,'show','634a3f8c067a3714a6a7c622015138af096ab5d2:libs/network/BleKeyboardHost/include/BleRemoteProfiles.h'],timeout=30))
 cpp=evidence/'runtime.cpp';cpp.write_text(r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include "Fakes.h"
int main(){
 fake::reset();bleturner::Config config;config.enabled=1;std::strcpy(config.peerAddr,"7d:de:5c:bd:ae:ca");
 bleturner::begin(fake::hostFns(),config);bleturner::tick(fake::reading(1));assert(fake::radio().running);
 // Shutdown failure is observable: no fabricated successful ownership handoff.
 fake::radio().endResult=false;assert(!bleturner::stopNow(20));assert(fake::radio().stopping);
 fake::radio().endResult=true;assert(bleturner::stopNow(20));assert(!fake::radio().running);
 assert(bleturner::beforeScreenChange(0,false));
 // A queued start is canceled before AirDrop/WiFi may take ownership.
 fake::reset();bleturner::begin(fake::hostFns(),config);fake::radio().queueStarts=true;
 bleturner::tick(fake::reading(2));assert(fake::radio().startQueued);
 assert(!bleturner::beforeScreenChange(0,false));fake::radio().runStart();
 assert(bleturner::beforeScreenChange(0,false));assert(!fake::radio().running);
 // No unrelated Sync/WiFi symbols are linked into this complete production runtime.
 assert(fake::host().delivered.empty());puts("complete production BLE runtime ownership PASS");
}
''')
 lib=ROOT/'lib/BlePageTurner'
 cmd=['g++','-std=c++20','-DBLETURNER_TESTING','-Wall','-Wextra','-Werror',*['-I'+str(p) for p in [evidence,lib/'include',lib/'src',lib/'test']],str(cpp),str(lib/'src/Runtime.cpp'),str(lib/'test/Fakes.cpp'),'-o',str(evidence/'runtime')]
 if sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie']
 results=[]
 for name,args in [('compile-runtime',cmd),('execute-runtime',[str(evidence/'runtime')])]:
  p=subprocess.run(args,capture_output=True,text=True,timeout=60);(evidence/(name+'.log')).write_text(p.stdout+p.stderr);results.append({'name':name,'exit':p.returncode,'argv':args})
  if p.returncode:break
 (evidence/'results.json').write_text(json.dumps(results,indent=2));print(results)
 return int(any(x['exit'] for x in results))
