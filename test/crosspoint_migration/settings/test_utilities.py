#!/usr/bin/env python3
"""Run the actual string/URL and frozen UTF8 production TUs, without SDK doubles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from test_migration import ROOT
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
paths=['src/util/StringUtils.cpp','src/util/UrlUtils.cpp','lib/Utf8/Utf8.cpp']
with tempfile.TemporaryDirectory(prefix='settings-utilities-',dir=os.environ.get('TMPDIR')) as d:
    cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-I'+str(ROOT/'src'),'-I'+str(ROOT/'lib/Utf8'),str(Path(__file__).parent/'utilities.cpp')]+[str(ROOT/x) for x in paths]+['-o',str(Path(d)/'probe')]
    if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    c=subprocess.run(cmd,capture_output=True,text=True,timeout=60);(a.evidence/'compile.log').write_text(c.stdout+c.stderr)
    receipt={'command':cmd,'compile_exit':c.returncode,'sha256':{x:hashlib.sha256((ROOT/x).read_bytes()).hexdigest() for x in paths}}
    if c.returncode==0:
        r=subprocess.run([str(Path(d)/'probe')],capture_output=True,text=True,timeout=30);receipt['run_exit']=r.returncode;(a.evidence/'run.log').write_text(r.stdout+r.stderr)
    (a.evidence/'receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2));raise SystemExit(receipt.get('run_exit',c.returncode))
