#!/usr/bin/env python3
"""Bounded actual Library TUs over deterministic fault-injecting storage HAL.
The HAL is an in-memory filesystem adapter, not physical SD/FAT qualification.
"""
import argparse, hashlib, json, os, pathlib, subprocess, time, sys
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
out=pathlib.Path(a.evidence);out.mkdir(parents=True,exist_ok=True)
deps=pathlib.Path(os.environ.get('LIBRARY_GTEST_ROOT',str(pathlib.Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))/'library-finish-deps/googletest-1.17.0/googletest')))
build=pathlib.Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))/('library-finish-san' if a.sanitize else 'library-finish-host');build.mkdir(exist_ok=True)
receipt=[]
def run(name,cmd):
 t=time.monotonic();r=subprocess.run([str(x) for x in cmd],capture_output=True,text=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 (out/(name+'.log')).write_text(r.stdout+r.stderr);receipt.append({'name':name,'exit':r.returncode,'seconds':time.monotonic()-t,'argv':[str(x) for x in cmd]});(out/'receipt.json').write_text(json.dumps(receipt,indent=2))
 if r.returncode: print((r.stdout+r.stderr)[-9000:]);raise SystemExit(r.returncode)
 print(name,'PASS')
flags=['g++','-std=c++20','-g0','-O1','-pthread','-I'+str(deps/'include'),'-I'+str(deps)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
run('gtest',flags+['-c',deps/'src/gtest-all.cc','-o',build/'gtest.o'])
run('gtest-main',flags+['-c',deps/'src/gtest_main.cc','-o',build/'gtest-main.o'])
for suite,sources in {
 'library_builder':['LibraryBuilder','LibraryIndexFile','LibraryMetadataCache','LibrarySort','LibraryFormat','LibraryText'],
 'library_format':['LibraryFormat'], 'library_index_file':['LibraryIndexFile','LibraryFormat'],
 'library_text':['LibraryText'], 'library_input_buffer':[], 'library_recent_order':[],
}.items():
 tests=sorted((here/suite).glob('*Test.cpp'))
 includes=[here/suite/'stubs',root/'lib/LibraryIndex',root/'lib/Serialization',root/'lib/Utf8',root/'src/activities/library']
 cmd=flags+['-I'+str(x) for x in includes]+tests+[root/'lib/LibraryIndex'/(x+'.cpp') for x in sources]
 if suite in ['library_builder','library_text']:cmd+=[root/'lib/Utf8/Utf8.cpp']
 exe=build/suite
 if suite=='library_builder':cmd+=['-lexpat','-DLIBRARY_FIXTURE_PATH="'+str(here/'upstream-v2.idx')+'"']
 run(suite+'-compile',cmd+[build/'gtest.o',build/'gtest-main.o','-o',exe])
 run(suite,[exe,'--gtest_output=json:'+str(out/(suite+'.json'))])
for probe in ['ui_data','native_disk']:
 run(probe,[sys.executable,here/(probe+'.py'),'--evidence',out/probe]+(['--sanitize'] if a.sanitize else []))
hashes={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in list((root/'lib/LibraryIndex').glob('*'))+list((root/'src/activities/library').glob('*'))+list((root/'src/components/icons').glob('libraryIcons.*')) if x.is_file()}
(out/'sources.json').write_text(json.dumps(hashes,indent=2))
print('All six actual-production suites, SDK UI-data adapter and native-disk integration passed')
