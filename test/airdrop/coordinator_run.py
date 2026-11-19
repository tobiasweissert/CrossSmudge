#!/usr/bin/env python3
from pathlib import Path
import argparse,re,subprocess,tempfile,json,hashlib
r=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
source=(r/'src/activities/network/AirDropReceiveActivity.cpp').read_text()
names=['routeConsentInput','resolveHandoff','onExit','beginLeaving','handleHomeGesture','setBrowserFailed','startBrowserHandoff','openBrowserChild','sameIdentity','decide','retryReceiver','loop']
methods=[]
for name in names:
 m=re.search(r'(?:void|bool) AirDropReceiveActivity::'+name+r'\([^)]*\) \{',source);assert m,name
 start=m.start();i=source.index('{',start);depth=1;j=i+1
 while depth:
  if source[j]=='{':depth+=1
  if source[j]=='}':depth-=1
  j+=1
 methods.append(source[start:j])
with tempfile.TemporaryDirectory(prefix='airdrop-coordinator-')as td:
 td=Path(td);(td/'coordinator_extracted.inc').write_text('\n'.join(methods))
 includes=[r/'test/airdrop/coordinator_stubs',r/'test/airdrop/browser_stubs',r/'test/browser/activity_stubs',r/'src/activities/apps/lua',r/'src',r/'lib/AirDrop',r/'lib/AirDrop/browser',td]
 cmd=['g++','-std=c++20','-O1','-g',*[f'-I{x}'for x in includes],str(r/'test/airdrop/coordinator_test.cpp'),str(r/'lib/AirDrop/browser/BrowserLaunch.cpp'),str(r/'lib/AirDrop/core/Offer.cpp'),'-o',str(td/'test')]
 if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(cmd,check=True);subprocess.run([str(td/'test')],check=True)
 print(json.dumps({'exit':0,'production_sha256':hashlib.sha256(source.encode()).hexdigest(),'methods':names,'lua_activity_sha256':hashlib.sha256((r/'src/activities/apps/lua/LuaAppActivity.h').read_bytes()).hexdigest(),'sanitized':a.sanitize,'scope':'actual coordinator methods/callbacks + actual LuaAppActivityAdapter/shared Launch; explicit VM/storage/receiver/device/allocation seams, not hardware proof'}))
