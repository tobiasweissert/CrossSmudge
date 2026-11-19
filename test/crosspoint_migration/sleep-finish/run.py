#!/usr/bin/env python3
"""Bounded real-body orchestration tests. Hardware, files, bitmap/PNG codecs are doubles."""
from pathlib import Path
import argparse, hashlib, json, subprocess, time
here=Path(__file__).resolve().parent
root=here.parents[2]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
src=root/'src/activities/boot_sleep/SleepActivity.cpp'
s=src.read_text();helpers=s[s.index('HalDisplay::GrayscaleMode sleepGrayscaleMode'):s.index('void releaseSdFontCachesForDecode')]
modern=s[s.index('void SleepActivity::renderModernBitmapSleepScreen'):]
cover=s[s.index('void SleepActivity::renderCoverSleepScreen()'):s.index('void SleepActivity::renderReadingStatsSleepScreen()')]
gray=(root/'lib/GfxRenderer/BitmapHelpers.h').read_text();gray=gray[gray.index('struct GrayPlanePixel'):gray.index('enum class BmpRowOrder')]
boundaries=(here/'boundaries.h').read_text().replace('/* GRAY_PLANE_PRODUCTION */',gray)
import re
settings=root/'src/CrossPointSettings.h'
enum_body=settings.read_text().split('enum SLEEP_SCREEN_MODE {',1)[1].split('};',1)[0]
boundaries=re.sub(r'enum SLEEP_SCREEN_MODE \{[^}]*\}',lambda _: 'enum SLEEP_SCREEN_MODE {'+enum_body+'}',boundaries)
cpp=a.evidence/'composition-body.cpp';cpp.write_text(boundaries+'\n'+helpers+'\n'+modern+'\n'+cover+'\n'+(here/'composition.cpp').read_text())
commands=[['python3',str(here/'preservation.py')],['python3',str(here/'dispatch.py'),str(a.evidence)]+(['--sanitize'] if a.sanitize else []),['g++','-std=c++17','-O0','-g',str(cpp),'-o',str(a.evidence/'composition')]+(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []),[str(a.evidence/'composition')]]
receipts=[]
for i,cmd in enumerate(commands):
 t=time.monotonic();res=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
 (a.evidence/f'{i}.log').write_text(res.stdout);print(res.stdout,end='')
 receipts.append(dict(command=cmd,exit=res.returncode,seconds=time.monotonic()-t))
 (a.evidence/'receipts.json').write_text(json.dumps(receipts,indent=2)+'\n')
 if res.returncode:raise SystemExit(res.returncode)
paths=[src,settings,root/'src/activities/boot_sleep/SleepActivity.h',root/'lib/GfxRenderer/BitmapHelpers.h',*sorted(here.glob('*'))]
(a.evidence/'inputs.json').write_text(json.dumps({str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths if f.is_file()},indent=2)+'\n')
print('All bounded orchestration tests passed; target build and physical grayscale remain unqualified.')
