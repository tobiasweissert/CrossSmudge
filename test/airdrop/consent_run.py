#!/usr/bin/env python3
"""Production HTTP and one-use consent fixtures; no networking/device claims."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
temporary=None
if os.environ.get('AIRDROP_TEST_OUTPUT'):
    OUT=Path(os.environ['AIRDROP_TEST_OUTPUT'])
else:
    temporary=tempfile.TemporaryDirectory(prefix='airdrop-consent-')
    OUT=Path(temporary.name)
OUT.mkdir(parents=True,exist_ok=True)
mode='sanitized' if '--sanitize' in sys.argv else 'normal'
env=dict(os.environ)
if mode=='sanitized':
    # Sandbox ptrace restrictions make LeakSanitizer unavailable; keep ASan+UBSan active.
    env['ASAN_OPTIONS']='detect_leaks=0'
exe=OUT/('consent-cases-'+mode)
cmd=['c++','-std=c++17','-g','-O1','-Wall','-Wextra','-Werror','-Ilib/AirDrop/core','test/airdrop/consent_cases.cpp','lib/AirDrop/core/Offer.cpp','lib/AirDrop/core/HttpRequest.cpp','lib/AirDrop/core/Consent.cpp','-o',str(exe)]
if mode=='sanitized':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
with (OUT/('consent-'+mode+'.log')).open('w') as log:
    print('COMMAND',json.dumps(cmd),file=log,flush=True)
    result=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env=env).returncode
    if result==0:
        print('COMMAND',json.dumps([str(exe)]),file=log,flush=True)
        result=subprocess.run([str(exe)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env=env).returncode
if result:
    print((OUT/('consent-'+mode+'.log')).read_text(),file=sys.stderr)
print('consent',mode,'exit',result)
sys.exit(result)
