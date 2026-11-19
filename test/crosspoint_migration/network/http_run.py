#!/usr/bin/env python3
"""Compile complete production HttpDownloader over bounded ESP/storage doubles."""
from pathlib import Path
import subprocess,sys,json,hashlib
R=Path(__file__).resolve().parents[3]
T=Path(__file__).resolve().parent
O=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/network/tests/http')
O.mkdir(parents=True,exist_ok=True)
for name in ['HalStorage.h','Stream.h','Arduino.h','Logging.h','Memory.h','WiFi.h','base64.h','esp_crt_bundle.h','esp_http_client.h','AppVersion.h','network/WifiPowerSaveGuard.h']:
 p=O/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n#include "'+str(T/'stubs.h')+'"\n')
sync=(R/'lib/KOReaderSync/KOReaderSyncClient.cpp').read_text()
a=sync.index('class SyncHttpClient {');b=sync.index('\n};',a)+3
(O/'sync.inc').write_text(sync[a:b])
mode='sanitize' if '--sanitize' in sys.argv else 'normal'
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-I'+str(O),'-I'+str(T),'-I'+str(R/'src/network'),'-I'+str(R/'src'),str(T/'http.cpp'),str(R/'src/network/HttpDownloader.cpp'),'-o',str(O/mode)]
if mode=='sanitize':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
p=subprocess.run(cmd,capture_output=True,text=True,timeout=45);log='$ '+' '.join(cmd)+'\n'+p.stdout+p.stderr
rc=p.returncode
if not rc:
 p=subprocess.run([str(O/mode)],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
(O/(mode+'.log')).write_text(log);print(log)
inputs=[T/'stubs.h',T/'http.cpp',Path(__file__),R/'src/network/HttpDownloader.h',R/'src/network/HttpDownloader.cpp',R/'src/network/HttpRedirectPolicy.h',R/'lib/KOReaderSync/KOReaderSyncClient.cpp',O/'sync.inc']
(O/(mode+'.json')).write_text(json.dumps({'returncode':rc,'command':cmd,'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2))
sys.exit(rc)
