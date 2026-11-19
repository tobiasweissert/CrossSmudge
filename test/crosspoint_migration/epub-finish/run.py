#!/usr/bin/env python3
"""Bounded whole-production-TU host probes; no dependency installs."""
import argparse, pathlib, subprocess, json, os, hashlib, time
T=pathlib.Path(__file__).resolve().parent; R=T.parents[2]
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True,type=pathlib.Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.evidence.resolve();O.mkdir(parents=True,exist_ok=True);receipts=[]
def run(cmd):
 cmd=list(map(str,cmd));start=time.monotonic()
 try:
  x=subprocess.run(cmd,cwd=R,capture_output=True,text=True,timeout=115,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1','UBSAN_OPTIONS':'halt_on_error=1'});log=x.stdout+x.stderr;rc=x.returncode
 except subprocess.TimeoutExpired as ex: log=str(ex);rc=124
 f=O/(str(len(receipts))+'.log');f.write_text(log);receipts.append({'command':cmd,'exit':rc,'seconds':round(time.monotonic()-start,3),'log':str(f)});(O/'receipts.json').write_text(json.dumps(receipts,indent=2)+'\n');print(log[-12000:])
 if rc:raise SystemExit(rc)
flags=['-std=c++20','-O1','-g0','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-pie','-no-pie']
for q in [T/'stubs',R/'test/browser/image_stubs',R/'test/memory_policy/stubs',R/'lib/Epub',R/'lib/Epub/Epub/parsers',R/'lib/Memory',R/'lib/MemoryBudget',R/'lib/Serialization',R/'lib/Utf8',R/'lib/XmlParserUtils']:
 flags+=['-I'+str(q)]
src=[T/'metadata.cpp',R/'lib/Epub/Epub/BookMetadataCache.cpp',R/'lib/Epub/Epub/parsers/ContentOpfParser.cpp',R/'lib/Utf8/Utf8.cpp']
run(['c++',*flags,*src,'-lexpat','-o',O/'metadata']);run([O/'metadata'])
(O/'inputs.json').write_text(json.dumps({str(q.relative_to(R)):hashlib.sha256(q.read_bytes()).hexdigest() for q in src},indent=2)+'\n')
(O/'metadata').unlink()
xtc=[T/'xtc.cpp',R/'lib/Xtc/Xtc.cpp',R/'lib/Xtc/Xtc/XtcParser.cpp',R/'lib/GfxRenderer/Bitmap.cpp',R/'lib/GfxRenderer/BitmapHelpers.cpp']
run(['c++',*flags,'-I'+str(R/'test/crosspoint_migration/reader-core/stubs'),'-I'+str(R/'lib/Xtc'),'-I'+str(R/'lib/GfxRenderer'),*xtc,'-o',O/'xtc']);run([O/'xtc'])
(O/'xtc-inputs.json').write_text(json.dumps({str(q.relative_to(R)):hashlib.sha256(q.read_bytes()).hexdigest() for q in xtc},indent=2)+'\n')
(O/'xtc').unlink()
