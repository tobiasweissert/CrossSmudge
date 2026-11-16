#!/usr/bin/env python3
"""Production launch ownership, package resolution, Lua activity and Browser startup."""
import datetime
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
temporary=None
if os.environ.get('AIRDROP_BROWSER_TEST_OUTPUT'):
    OUT=Path(os.environ['AIRDROP_BROWSER_TEST_OUTPUT'])
else:
    temporary=tempfile.TemporaryDirectory(prefix='airdrop-browser-')
    OUT=Path(temporary.name)
mode='sanitized' if '--sanitize' in sys.argv else 'normal'
OUT=OUT/mode
OUT.mkdir(parents=True,exist_ok=True)
includes=[os.environ.get('ARDUINOJSON_INCLUDE',''),*(str(ROOT/'.pio/libdeps'/name/'ArduinoJson/src')for name in ('sticky','sticky-owner-test'))]
JSON=next((Path(p)for p in includes if p and Path(p,'ArduinoJson.h').is_file()),None)
if JSON is None:
    raise SystemExit('Set ARDUINOJSON_INCLUDE to the pinned ArduinoJson/src already provisioned; this test never downloads dependencies')
env=dict(os.environ,SMUDGE_X3_CONSTRAINTS='1',ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
flags=['-O1','-g','-fno-omit-frame-pointer']
if mode=='sanitized':flags+=['-fsanitize=address,undefined']
common=[*flags,'-Ilib/AirDrop/browser','-Ilib/Lua/src','-Ilib/LuaHttp','-Isrc/activities/apps/lua','-Ilib/LuaBrowser','-Ilib/Memory']
receipts=[]
with (OUT/'commands.log').open('w')as log:
    def run(command):
        command=list(map(str,command))
        print('COMMAND',json.dumps(command),file=log,flush=True)
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
        receipts.append({'command':command,'exit':result.returncode})
        (OUT/'results.json').write_text(json.dumps({'mode':mode,'environment':{k:env[k]for k in ['SMUDGE_X3_CONSTRAINTS','ASAN_OPTIONS','UBSAN_OPTIONS']},'commands':receipts},indent=2)+'\n')
        if result.returncode:
            log.flush();print((OUT/'commands.log').read_text(),file=sys.stderr)
            raise SystemExit(result.returncode)
    core=['lib/AirDrop/browser/BrowserLaunch.cpp','lib/AirDrop/core/Offer.cpp']
    run(['c++','-std=c++20',*common,'test/airdrop/browser_launch.cpp',*core,'-o',OUT/'contract'])
    run([OUT/'contract'])
    activity=['c++','-std=c++20',*common,'-Itest/airdrop/browser_stubs','-Itest/browser/activity_stubs','-Isrc','test/airdrop/browser_activity.cpp',*core,'-o',OUT/'activity']
    run(activity);run([OUT/'activity'])
    for extra,name in [(['-DSIMULATOR=1'],'package'),(['-DTEST_NO_PSRAM=1'],'package-c3')]:
        run(['c++','-std=c++20',*common,*extra,'-Itest/airdrop/browser_stubs','-I'+str(JSON),'test/airdrop/browser_package.cpp','-o',OUT/name]);run([OUT/name])
    objects=[]
    for source in sorted((ROOT/'lib/Lua/src').glob('l*.c')):
        obj=OUT/(source.stem+'.o');run(['cc',*common,'-c',source,'-o',obj]);objects.append(obj)
    run(['c++','-std=c++20',*common,'test/airdrop/browser_runner.cpp',*core,'lib/AirDrop/browser/LuaLaunch.cpp',*objects,'-lm','-ldl','-o',OUT/'lua'])
    for suite in ['test/airdrop/browser_startup.lua','test/browser/parser.lua','test/browser/layout.lua','test/browser/integration.lua','test/browser/work.lua','test/browser/repair.lua']:
        run([OUT/'lua',suite])
print('PASS',mode,'Browser native launch/package/production activity/async startup and existing parser/layout/integration/work/repair')
