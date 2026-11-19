#!/usr/bin/env python3
"""Actual production index/ZIP/miniz/Expat over real isolated Linux disk I/O.
No target SD/FAT, radio, or hardware timing claims.
"""
import argparse,json,os,shutil,subprocess,tempfile,zipfile,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[2];out=Path(a.evidence);out.mkdir(parents=True,exist_ok=True)
build=Path(tempfile.mkdtemp(prefix='library-native-',dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch')))
sandbox=build/'sd';sandbox.mkdir()
for d in ['.crosspoint/cache_1','apps/game','fonts','sleep','books']:(sandbox/d).mkdir(parents=True,exist_ok=True)
shutil.copyfile(here/'upstream-v2.idx',sandbox/'.crosspoint/library.idx')
for f in ['a.txt','b.txt']:(sandbox/f).write_text(f[0])
for f in ['.crosspoint/recent.json','.crosspoint/cache_1/progress','.crosspoint/cache_1/bookmarks','.crosspoint/cache_1/clippings','.crosspoint/cache_1/reading_stats','.crosspoint/cache_1/reading_stats_off','apps/game/save.txt','fonts/readme.txt','sleep/private.txt']:(sandbox/f).write_text('durable-test-canary:'+f)
with zipfile.ZipFile(sandbox/'template.bin','w',compression=zipfile.ZIP_DEFLATED) as z:
 z.writestr('META-INF/container.xml','<container><rootfiles><rootfile full-path="OEBPS/content.opf"/></rootfiles></container>')
 z.writestr('OEBPS/content.opf','<package xmlns:dc="urn:dc"><metadata><dc:title>Native ZIP title</dc:title><dc:creator>Native Author</dc:creator><dc:subject>History</dc:subject><meta name="calibre:series" content="Native Saga"/><meta name="calibre:series_index" content="2.5"/></metadata><manifest/></package>')
cmd=['g++','-std=c++20','-O1','-g0','-DSIMULATOR','-I'+str(here/'disk_stubs'),'-I'+str(root/'lib/Memory'),'-I'+str(root/'lib/MemoryBudget'),'-I'+str(root/'lib/LibraryIndex'),'-I'+str(root/'lib/Utf8'),'-I'+str(root/'lib/Serialization'),'-I'+str(root/'lib/ZipFile'),'-I'+str(root/'lib/miniz/src'),'-I'+str(here/'library_builder/stubs')]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
cmd+=[str(here/'native_disk.cpp')]+[str(root/'lib/LibraryIndex'/(n+'.cpp')) for n in ['LibraryBuilder','LibraryIndexFile','LibraryMetadataCache','LibrarySort','LibraryFormat','LibraryText']]+[str(root/f) for f in ['lib/Utf8/Utf8.cpp','lib/ZipFile/ZipFile.cpp','lib/miniz/src/InflateStream.cpp','lib/miniz/src/miniz_impl.c','lib/Memory/BuildScratch.cpp']]+['-lexpat','-o',str(build/'native-disk')]
receipt=[]
inputs={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for d in ['lib/ZipFile','lib/miniz/src','lib/Memory','lib/MemoryBudget','lib/Utf8','lib/Serialization','lib/LibraryIndex'] for p in (root/d).rglob('*') if p.is_file() and p.suffix in ['.h','.cpp','.c']}
(out/'source-inputs.json').write_text(json.dumps(inputs,indent=2))
for name,command in [('compile',cmd),('run',[str(build/'native-disk'),str(sandbox)])]:
 r=subprocess.run(command,capture_output=True,text=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(name+'.log')).write_text(r.stdout+r.stderr);receipt.append({'name':name,'exit':r.returncode,'argv':command});(out/'receipt.json').write_text(json.dumps(receipt,indent=2));print(name,r.returncode,(r.stdout+r.stderr)[-14000:]);
 if r.returncode:raise SystemExit(1)
assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items()),'Production inputs changed during qualification'
shutil.rmtree(sandbox)
