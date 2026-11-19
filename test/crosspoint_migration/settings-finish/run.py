#!/usr/bin/env python3
"""Bounded production-TU storage and global security qualification; no SDK/build/install."""
from pathlib import Path
import argparse,hashlib,json,subprocess
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
mode='sanitize' if a.sanitize else 'normal'
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-unused-function','-fno-exceptions','-fno-rtti','-g','-I'+str(T/'stubs'),'-I'+str(R/'src'),'-I'+str(R/'lib/Serialization'),str(T/'clipping.cpp'),str(R/'src/ClippingStore.cpp'),'-o',str(a.evidence/'clipping')]
if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=90);log=r.stdout+r.stderr;rc=r.returncode
if not rc:
 r=subprocess.run([str(a.evidence/'clipping')],capture_output=True,text=True,timeout=25);log+=r.stdout+r.stderr;rc=r.returncode
(a.evidence/'clipping.log').write_text(log)
inputs=[R/'src/ClippingStore.cpp',R/'src/ClippingStore.h',T/'clipping.cpp',Path(__file__),*sorted((T/'stubs').glob('*'))]
receipt={'mode':mode,'command':cmd,'returncode':rc,'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
(a.evidence/'clipping.json').write_text(json.dumps(receipt,indent=2));print(log);raise SystemExit(rc)
