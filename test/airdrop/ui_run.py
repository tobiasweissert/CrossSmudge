#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,tempfile,re,hashlib,json,os
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');args=p.parse_args()
source=(root/'src/activities/network/AirDropReceiveActivity.cpp').read_text()
names=['routeConsentInput','sameIdentity','decide','beginLeaving','handleHomeGesture','deferGlobalPowerAction','resolveHandoff','onExit','loop','retryReceiver']
methods=[]
for name in names:
 m=re.search(r'(?:bool|void) AirDropReceiveActivity::'+name+r'\([^)]*\) \{',source);assert m,name
 a=m.start();i=source.index('{',a);depth=1;j=i+1
 while depth:
  if source[j]=='{':depth+=1
  if source[j]=='}':depth-=1
  j+=1
 methods.append(source[a:j])
# No production reset path exists; the web parent has an explicit tested native branch.
assert not re.search(r'silentRestart|ESP\.restart|esp_restart|vTaskDelete',source)
with tempfile.TemporaryDirectory(prefix='airdrop-ui-lifecycle-')as td:
 td=Path(td);(td/'ui_lifecycle_extracted.inc').write_text('\n'.join(methods))
 cmd=['g++','-std=c++20','-g','-O1','-Wall','-Wextra','-Werror','-I'+str(root/'src'),'-I'+str(root/'lib/AirDrop'),'-I'+str(td),str(root/'test/airdrop/ui_lifecycle_test.cpp'),str(root/'lib/AirDrop/browser/BrowserLaunch.cpp'),str(root/'lib/AirDrop/core/Offer.cpp'),'-o',str(td/'test')]
 if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer',*(['-no-pie'] if os.name!='nt' and os.uname().sysname!='Darwin' else [])]
 subprocess.run(cmd,check=True);subprocess.run([str(td/'test')],check=True)
 print(json.dumps({'exit':0,'production_sha256':hashlib.sha256(source.encode()).hexdigest(),'methods':names,'sanitized':args.sanitize}))

subprocess.run(['python3',str(root/'test/airdrop/consent_epoch_run.py'),*(['--sanitize'] if args.sanitize else [])],check=True)
