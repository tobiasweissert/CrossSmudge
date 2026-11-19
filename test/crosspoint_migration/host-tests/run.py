#!/usr/bin/env python3
"""Bounded real GoogleTest host units; records commands, hashes and exit receipts.
No SDK initialization or downloads. Use --unit all only after source writer freeze.
"""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, time
R=pathlib.Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--out',required=True);p.add_argument('--gtest',required=True);p.add_argument('--sdk-json',required=True);p.add_argument('--unit',default='release');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
O=pathlib.Path(a.out);O.mkdir(parents=True,exist_ok=True);G=pathlib.Path(a.gtest);S=pathlib.Path(a.sdk_json)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
base=['g++','-std=c++20','-O0','-g0','-pthread','-I'+str(R),'-I'+str(R/'lib'),'-I'+str(G/'googletest/include'),'-I'+str(G/'googletest')]
if a.sanitize:base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
def execute(cmd,log):
 try:
  with (O/log).open('w') as f:ret=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'}).returncode
 except subprocess.TimeoutExpired:ret=124
 return {'command':cmd,'exit':ret,'log':log}
units={
 'release':(['test/release_json_parser/ReleaseJsonParserTest.cpp','lib/JsonParser/ReleaseJsonParser.cpp',str(S/'StreamingJsonParser.cpp')],['lib/JsonParser',str(S)]),
 'grayscale':(['test/absolute_grayscale/AbsoluteGrayscaleTest.cpp'],[]),
 'clipping':(['test/clipping_preview/ClippingPreviewTest.cpp'],['src','lib/Epub']),
 'rounding':(['test/differential_rounding/DifferentialRoundingTest.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp'],['lib/EpdFont','lib/Utf8']),
 'fs':(['test/fs_helpers/FsHelpersTest.cpp','lib/FsHelpers/FsHelpers.cpp','lib/FsHelpers/NaturalSort.cpp','lib/Utf8/Utf8.cpp'],['test/fs_helpers/stubs','lib/FsHelpers','lib/Utf8']),
}
results=[]
for unit in (list(units) if a.unit=='all' else a.unit.split(',')):
 sources,incs=units[unit];sources=[pathlib.Path(s) if pathlib.Path(s).is_absolute() else R/s for s in sources]
 sources += [G/'googletest/src/gtest-all.cc',G/'googletest/src/gtest_main.cc']
 # Ask the actual compiler for transitive user-header dependencies. Hashing
 # whole include trees falsely binds unrelated moving peer files; hashing TUs
 # alone misses the production header that supplies the tested behavior.
 inputs={str(s):digest(s) for s in sources}
 includes=['-I'+str(pathlib.Path(i) if pathlib.Path(i).is_absolute() else R/i) for i in incs]
 dep=subprocess.run(base+includes+['-MM']+[str(s) for s in sources],capture_output=True,text=True,timeout=120)
 (O/(unit+'-dependencies.log')).write_text(dep.stdout+dep.stderr)
 if dep.returncode==0:
  import shlex
  for line in dep.stdout.replace('\\\n',' ').splitlines():
   if ': ' not in line: continue
   for name in shlex.split(line.split(': ',1)[1]):
    h=pathlib.Path(name)
    if h.is_file(): inputs[str(h.resolve())]=digest(h)
 cmd=base+includes+[str(s) for s in sources]+['-o',str(O/unit)]
 receipt={'unit':unit,'sanitize':a.sanitize,'dependency_exit':dep.returncode,'inputs':inputs,'compile':execute(cmd,unit+'-compile.log')}
 if receipt['compile']['exit']==0:receipt['run']=execute([str(O/unit)],unit+'-run.log')
 receipt['inputs_unchanged']=all(pathlib.Path(f).is_file() and digest(pathlib.Path(f))==h for f,h in inputs.items())
 (O/(unit+'.json')).write_text(json.dumps(receipt,indent=2)+'\n');results.append(receipt)
 print(unit,'compile',receipt['compile']['exit'],'run',receipt.get('run',{}).get('exit'),'unchanged',receipt['inputs_unchanged'],flush=True)
(O/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(int(any(t['dependency_exit'] or t['compile']['exit'] or t.get('run',{}).get('exit',1) or not t['inputs_unchanged'] for t in results)))
