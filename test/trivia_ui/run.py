#!/usr/bin/env python3
"""Exact int32/float32 Lua + production raster, libraries and Sticky identity."""
from pathlib import Path
import subprocess,json,hashlib,argparse,shutil,sys
R=Path(__file__).resolve().parents[2]
DEFAULT_EVIDENCE=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/trivia-ui')
p=argparse.ArgumentParser()
p.add_argument('--work-dir',type=Path,default=DEFAULT_EVIDENCE)
p.add_argument('--output',type=Path)
p.add_argument('--app-dir',type=Path,default=R/'apps/cp-trivia')
p.add_argument('--test',help='One focused test; default runs all routine fixtures')
a=p.parse_args();H=a.work_dir.resolve();H.mkdir(parents=True,exist_ok=True)
if a.output is None:a.output=H/'results.json'
a.output.parent.mkdir(parents=True,exist_ok=True)
B=H/'build';B.mkdir(parents=True,exist_ok=True);shared=B/'raster'
subprocess.run([sys.executable,str(R/'test/crossplay_ui_parity/build_capture.py'),'--output',str(shared),'--root',str(R)],check=True)
# The canonical adapter opens the production offline libraries. This declared
# network app also needs the same lazy UTF-8 registration as its real manifest.
(B/'network.cpp').write_text('''#include "LuaRuntime.h"
extern "C" int trivia_open_libraries(lua_State*L){int rc=ink::runtime::openEssential(L);return rc?rc:ink::runtime::openNetwork(L,true);}
''')
c=(shared/'runner.c').read_text().replace('prod_open_libraries','trivia_open_libraries')
(B/'runner.c').write_text(c)
flags=['-I'+str(R/'lib/Lua/src')]
subprocess.run(['c++','-O2','-std=c++17',*flags,'-I'+str(R/'src/activities/apps/lua'),'-c',str(B/'network.cpp'),'-o',str(B/'network.o')],check=True)
subprocess.run(['cc','-O2',*flags,'-c',str(B/'runner.c'),'-o',str(B/'runner.o')],check=True)
subprocess.run(['c++',str(B/'runner.o'),str(B/'network.o'),str(shared/'production_adapter.o'),str(shared/'production_identity.o'),*[str(shared/(p.stem+'.o'))for p in sorted((R/'lib/Lua/src').glob('*.c'))],'-lm','-o',str(B/'runner')],check=True)
results=[]
for test in [a.test] if a.test else ['composition_contract.lua','behavior.lua','native_compare.lua']:
 for w,h in [(480,800),(800,480)]:
  app=H/'fixtures'/Path(test).stem/f'{w}x{h}';shutil.rmtree(app,ignore_errors=True);shutil.copytree(a.app_dir,app)
  t=subprocess.run([str(B/'runner'),str(R/'test/trivia_ui'/test),'2097152',str(w),str(h)],cwd=app,capture_output=True,text=True,timeout=120)
  metrics=json.loads(t.stdout.strip().splitlines()[-1]) if t.stdout.strip() else {}
  results.append(dict(test=test,width=w,height=h,returncode=t.returncode,stdout=t.stdout,stderr=t.stderr,metrics=metrics))
  print(test,w,h,t.returncode,t.stdout,t.stderr)
if not a.test:
 import struct,zlib
 metadata=json.loads((R/'test/trivia_ui/reference/sources.json').read_text())
 comparisons=[]
 for state in ['notice','choice','wrong','correct']:
  reference=R/'test/trivia_ui/reference'/(state+'-480x800.png')
  data=reference.read_bytes();assert hashlib.sha256(data).hexdigest()==metadata['pixels'][reference.name]
  offset=8;compressed=b''
  while offset<len(data):
   length=struct.unpack('>I',data[offset:offset+4])[0];kind=data[offset+4:offset+8];body=data[offset+8:offset+8+length];offset+=length+12
   if kind==b'IHDR':width,height,depth,color,_,_,interlace=struct.unpack('>IIBBBBB',body);assert (width,height,depth,color,interlace)==(480,800,8,0,0)
   if kind==b'IDAT':compressed+=body
  raw=zlib.decompress(compressed);pixels=b''
  for row in range(height):
   assert raw[row*(width+1)]==0,'reference encoder changed row filter'
   pixels+=raw[row*(width+1)+1:(row+1)*(width+1)]
  pgm=H/'fixtures/native_compare/480x800'/('native-'+state+'.pgm')
  _,wh,_,actual=pgm.read_bytes().split(b'\n',3);assert wh==b'480 800'
  regions={'full_notice':metadata['full_notice_comparison']}if state=='notice'else metadata['comparison_regions']
  for region,(x0,y0,x1,y1)in regions.items():
   differences=sum(pixels[y*width+x]!=actual[y*width+x]for y in range(y0,y1)for x in range(x0,x1))
   comparisons.append(dict(state=state,region=region,different_pixels=differences,total_pixels=(x1-x0)*(y1-y0)))
   assert differences==0,(state,region,differences)
 print('PASS native portrait pixel regions',comparisons)
 (H/'pixel-regions.json').write_text(json.dumps({'comparisons':comparisons,'exclusions':metadata['exclusions']},indent=2)+'\n')
payload=dict(results=results,identity='actual l_getDevice compiled with configured sticky-owner-test defines; '+str(shared/'identity-proof.json'),libraries='actual LuaRuntime openEssential + openNetwork(true)',raster='reference-tooling frozen extracted production shapes/bitmap, not hardware',renderer_metadata=str(shared/'production-methods.json'),source_hashes={str(p.relative_to(a.app_dir)):hashlib.sha256(p.read_bytes()).hexdigest()for p in a.app_dir.rglob('*')if p.is_file()})
a.output.write_text(json.dumps(payload,indent=2)+'\n')
assert all(t['returncode']==0 and t['metrics']['after_close']==0 for t in results)
