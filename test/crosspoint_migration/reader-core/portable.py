#!/usr/bin/env python3
"""Dependency-free production CSS and uzlib cores, explicit hardware doubles."""
import argparse, pathlib, subprocess, json, os, hashlib, time
T=pathlib.Path(__file__).parent;R=T.parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);receipts=[];inputs=set()
def run(cmd):
 start=time.monotonic();cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=R,capture_output=True,text=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1','UBSAN_OPTIONS':'halt_on_error=1'});log=p.stdout+p.stderr;f=O/(str(len(receipts))+'.log');f.write_text(log);receipts.append({'command':cmd,'exit':p.returncode,'seconds':round(time.monotonic()-start,3),'log':str(f)});(O/'receipts.json').write_text(json.dumps(receipts,indent=2)+'\n');print(log[-8000:]);
 if p.returncode:raise SystemExit(p.returncode)
flags=['-O1','-g0','-fno-omit-frame-pointer']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-pie','-no-pie']
for p in [T/'stubs',R/'test/browser/image_stubs',R/'test/memory_policy/stubs',R/'lib/Epub',R/'lib/Memory',R/'lib/AsciiText',R/'lib/InflateReader',R/'lib/uzlib/src']:flags+=['-I'+str(p)]
css=[T/'css.cpp',R/'lib/Epub/Epub/css/CssParser.cpp']
run(['c++','-std=c++20',*flags,*css,'-o',O/'css']);run([O/'css']);inputs.update(css)
objects=[]
for src in [R/'lib/uzlib/src/tinflate.c',R/'lib/uzlib/src/uzlib_checksums.c']:
 obj=O/(src.name+'.o');run(['cc',*flags,'-c',src,'-o',obj]);objects.append(obj);inputs.add(src)
sources=[T/'inflate.cpp',R/'lib/InflateReader/InflateReader.cpp'];run(['c++','-std=c++20',*flags,*sources,*objects,'-lz','-o',O/'inflate']);run([O/'inflate']);inputs.update(sources)
# Type-check all pointwise compatibility units without invoking a target build.
for src in [R/'lib/Epub/Epub/hyphenation/Hyphenator.cpp',R/'lib/Epub/Epub/hyphenation/LanguageRegistry.cpp']:
 run(['c++','-std=c++20',*flags,'-I'+str(R/'lib/Utf8'),'-I'+str(R/'lib/MemoryBudget'),'-fsyntax-only',src]);inputs.add(src)
# TXT compiles as the complete production translation unit; unrelated cover routines
# are linker-discarded, and the unresolved metadata writer is explicitly doubled.
txt=[T/'txt.cpp',R/'lib/Txt/Txt.cpp',R/'lib/Txt/TxtToHtml.cpp',R/'lib/Utf8/Utf8.cpp']
run(['c++','-std=c++20',*flags,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(R/'lib/Txt'),'-I'+str(R/'lib/Utf8'),'-I'+str(R/'lib/Serialization'),'-I'+str(R/'lib/JpegToBmpConverter'),'-I'+str(R/'lib/PngToBmpConverter'),*txt,'-o',O/'txt']);run([O/'txt']);inputs.update(txt)
(O/'inputs.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},indent=2)+'\n')
for f in [*objects,O/'css',O/'inflate',O/'txt']:f.unlink()
print('PASS portable production CSS, render spec, block style, inflate; hyphenation syntax; '+('ASan/UBSan' if a.sanitize else 'normal'))
