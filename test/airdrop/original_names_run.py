#!/usr/bin/env python3
"""Original-name production storage regressions; memory I/O, no real SD."""
import argparse, json, os, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--sanitize', action='store_true')
p.add_argument('--case', default='all', choices=['all','original','recovery','collision','journals','policy','archive'])
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
flags=['-O1','-g','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations','-fno-exceptions','-fno-rtti']
if a.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
with tempfile.TemporaryDirectory(prefix='airdrop-names-') as tmp:
    obj=str(Path(tmp)/'miniz.o'); exe=str(Path(tmp)/'names')
    commands=[['cc','-std=c11',*[f for f in flags if f!='-fno-rtti'],'-Wno-error','-Ilib/miniz/src','-c','lib/miniz/src/miniz_impl.c','-o',obj],['c++','-std=c++20',*flags,'-Ilib/AirDrop/storage','-Ilib/miniz/src','test/airdrop/original_names_test.cpp','lib/AirDrop/storage/StorageSink.cpp','lib/AirDrop/stream/StreamDecoder.cpp',obj,'-lcrypto','-lz','-o',exe]]
    commands += [[exe,case] for case in (['original','recovery','collision','journals','policy','archive'] if a.case=='all' else [a.case])]
    for cmd in commands:
        print(json.dumps(cmd),flush=True)
        result=subprocess.run(cmd,cwd=r,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
        if result.returncode: raise SystemExit(result.returncode if result.returncode>0 else 1)
