#!/usr/bin/env python3
from pathlib import Path
import argparse,re,subprocess,tempfile,hashlib,json
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--source',type=Path,default=root/'src/main.cpp');a=p.parse_args()
source=a.source.read_text()
def function(text,pattern):
 m=re.search(pattern,text);assert m,pattern
 start=m.start();i=text.index('{',start);depth=1;j=i+1
 while depth:
  if text[j]=='{':depth+=1
  if text[j]=='}':depth-=1
  j+=1
 return text[start:j]
settings=(root/'src/CrossPointSettings.h').read_text()
enum=re.search(r'  enum SHORT_PWRBTN \{.*?\n  \};',settings,re.S).group()
activity=(root/'src/activities/network/AirDropReceiveActivity.cpp').read_text()
hook=function(activity,r'bool AirDropReceiveActivity::deferGlobalPowerAction\([^)]*\) \{')
dispatch=function(source,r'bool handleGlobalPowerButtonAction\([^)]*\) \{')
caller=function(source,r'bool dispatchShortcutAction\([^)]*\) \{')
start=source.index('bool exclusivePowerActionPending =');end=source.index('bool handleGlobalPowerButtonAction(',start)
globals=source[start:end]
# Parent may introduce the narrow action classifier; compile its exact production body too.
classifier=function((root/'src/GlobalActions.h').read_text(),r'bool isPowerButtonActionAvailableOutsideReader\([^)]*\) \{')+'\n'
m=re.search(r'bool isGlobalPowerButtonAction\([^)]*\) \{',source)
if m:classifier+=function(source,r'bool isGlobalPowerButtonAction\([^)]*\) \{')
replay=function(source,r'  if \(exclusivePowerActionPending\) \{')
assert source.index('mappedInputManager.update();',source.index('void loop()'))<source.index('if (exclusivePowerActionPending)',source.index('void loop()'))
with tempfile.TemporaryDirectory(prefix='radio-power-')as td:
 td=Path(td);(td/'radio_power_enum.inc').write_text('struct CrossPointSettings {\n'+enum+'\n};\n')
 (td/'radio_power_activity.inc').write_text(hook)
 (td/'radio_power_dispatch.inc').write_text(globals+'\n'+classifier+'\n'+dispatch+'\n'+caller)
 (td/'radio_power_replay.inc').write_text(replay)
 cmd=['g++','-std=c++20','-O1','-g','-I'+str(root/'src'),'-I'+str(td),str(root/'test/airdrop/radio_power_test.cpp'),'-o',str(td/'test')]
 if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(cmd,check=True);subprocess.run([str(td/'test')],check=True)
 print(json.dumps({'source':str(a.source),'sha256':hashlib.sha256(source.encode()).hexdigest(),'sanitized':a.sanitize,'scope':'exact production dispatcher+queue block+activity hook, mocked worker/hardware; no physical proof','exit':0}))
