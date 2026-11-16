#!/usr/bin/env python3
"""Exact int32/float32 Lua + production raster, libraries and Sticky identity."""
from pathlib import Path
import subprocess,json,hashlib,argparse,shutil,sys,struct,zlib
R=Path(__file__).resolve().parents[2]
DEFAULT_EVIDENCE=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/hackernews-ui')
p=argparse.ArgumentParser()
p.add_argument('--work-dir',type=Path,default=DEFAULT_EVIDENCE)
p.add_argument('--output',type=Path)
p.add_argument('--app-dir',type=Path,default=R/'apps/cp-hackernews')
p.add_argument('--test',help='One focused test; default runs all routine fixtures')
a=p.parse_args();H=a.work_dir.resolve();H.mkdir(parents=True,exist_ok=True)
if a.output is None:a.output=H/'results.json'
a.output.parent.mkdir(parents=True,exist_ok=True)
B=H/'build';B.mkdir(parents=True,exist_ok=True);shared=B/'raster'
subprocess.run([sys.executable,str(R/'test/crossplay_ui_parity/build_capture.py'),'--output',str(shared),'--root',str(R)],check=True)
# The canonical adapter opens the production offline libraries. This declared
# network app also needs the same lazy UTF-8 registration as its real manifest.
(B/'network.cpp').write_text('''#include "LuaRuntime.h"
extern "C" int hn_open_libraries(lua_State*L){int rc=ink::runtime::openEssential(L);return rc?rc:ink::runtime::openNetwork(L,true);}
''')
c=(shared/'runner.c').read_text().replace('prod_open_libraries','hn_open_libraries')
(B/'runner.c').write_text(c)
flags=['-I'+str(R/'lib/Lua/src')]
subprocess.run(['c++','-O2','-std=c++17',*flags,'-I'+str(R/'src/activities/apps/lua'),'-c',str(B/'network.cpp'),'-o',str(B/'network.o')],check=True)
subprocess.run(['cc','-O2',*flags,'-c',str(B/'runner.c'),'-o',str(B/'runner.o')],check=True)
subprocess.run(['c++',str(B/'runner.o'),str(B/'network.o'),str(shared/'production_adapter.o'),str(shared/'production_identity.o'),*[str(shared/(p.stem+'.o'))for p in sorted((R/'lib/Lua/src').glob('*.c'))],'-lm','-o',str(B/'runner')],check=True)
results=[]
for test in [a.test] if a.test else ['composition.lua','behavior.lua','status.lua','native_compare.lua','memory.lua']:
 for w,h in [(480,800),(800,480)]:
  app=H/'fixtures'/Path(test).stem/f'{w}x{h}';shutil.rmtree(app,ignore_errors=True);shutil.copytree(a.app_dir,app)
  script=R/'test/hackernews_original_ui'/test
  if test!='composition.lua':
   script=H/('fixture-'+test);script.write_text((R/'test/hackernews_original_ui/support.lua').read_text()+'\n'+(R/'test/hackernews_original_ui'/test).read_text())
  t=subprocess.run([str(B/'runner'),str(script),'2097152',str(w),str(h)],cwd=app,capture_output=True,text=True,timeout=120)
  metrics=json.loads(t.stdout.strip().splitlines()[-1]) if t.stdout.strip() else {}
  results.append(dict(test=test,width=w,height=h,returncode=t.returncode,stdout=t.stdout,stderr=t.stderr,metrics=metrics))
  print(test,w,h,t.returncode,t.stdout,t.stderr)
# Export exact full-size frame pixels; no host fonts or image substitutions.
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
for image in (H/'fixtures').rglob('*.pgm'):
 _,wh,_,pixels=image.read_bytes().split(b'\n',3);width,height=map(int,wh.split())
 path=H/'captures'/image.relative_to(H/'fixtures').with_suffix('.png');path.parent.mkdir(parents=True,exist_ok=True)
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pixels[y*width:(y+1)*width]for y in range(height))))+chunk(b'IEND',b''))
 for ext in ['.trace','.state.txt']:
  source=image.with_suffix(ext)
  if source.exists():shutil.copyfile(source,path.with_suffix(ext))
if not a.test:
 metadata=json.loads((R/'test/hackernews_original_ui/reference/sources.json').read_text())
 for name,expected in metadata['preserved_sha256'].items():assert hashlib.sha256((a.app_dir/name).read_bytes()).hexdigest()==expected,'accepted baseline changed: '+name
 legacy=(a.app_dir/'presentation.lua').read_text().split('\n',2)[2]
 assert hashlib.sha256(legacy.encode()).hexdigest()==metadata['legacy_presentation_sha256']
 comparisons=[]
 for state in ['busy','list','reader']:
  for width,height in [(480,800),(800,480)]:
   reference=R/'test/hackernews_original_ui/reference'/f'{state}-{width}x{height}.png';data=reference.read_bytes()
   assert hashlib.sha256(data).hexdigest()==metadata['sha256'][reference.name]
   offset=8;compressed=b''
   while offset<len(data):
    length=struct.unpack('>I',data[offset:offset+4])[0];kind=data[offset+4:offset+8];body=data[offset+8:offset+8+length];offset+=length+12
    if kind==b'IDAT':compressed+=body
   raw=zlib.decompress(compressed);expected=b''
   for row in range(height):assert raw[row*(width+1)]==0;expected+=raw[row*(width+1)+1:(row+1)*(width+1)]
   _,wh,_,actual=(H/'fixtures/native_compare'/f'{width}x{height}'/(state+'.pgm')).read_bytes().split(b'\n',3)
   assert wh==f'{width} {height}'.encode()
   regions={'full_frame':[0,0,width,height]}if state!='list'else {'main':[0,0,width,height-68],'footer_shape_top':[0,height-68,width,height-54],'footer_shape_bottom':[0,height-25,width,height]}
   for region,(x0,y0,x1,y1)in regions.items():
    differences=sum(expected[y*width+x]!=actual[y*width+x]for y in range(y0,y1)for x in range(x0,x1))
    comparisons.append(dict(state=state,width=width,height=height,region=region,different_pixels=differences,total_pixels=(x1-x0)*(y1-y0)))
    assert differences==0,(state,width,region,differences)
 (H/'pixel-results.json').write_text(json.dumps({'comparisons':comparisons,'scope':metadata['pixel_scope']},indent=2)+'\n')
 print('PASS exact native Busy/Reader and scoped List pixels in both orientations')
 subprocess.run([sys.executable,str(R/'test/hackernews_original_ui/legacy_repairs.py'),'--work-dir',str(H/'legacy-repairs'),'--runner',str(B/'runner'),'--app-dir',str(a.app_dir)],check=True)
 subprocess.run([sys.executable,str(R/'test/hackernews_original_ui/preservation.py'),'--work-dir',str(H/'preservation'),'--runner',str(B/'runner'),'--app-dir',str(a.app_dir)],check=True)
payload=dict(results=results,identity='actual l_getDevice compiled with configured sticky-owner-test defines; '+str(shared/'identity-proof.json'),libraries='actual LuaRuntime openEssential + openNetwork(true)',raster='reference-tooling frozen extracted production shapes/bitmap, not hardware',renderer_metadata=str(shared/'production-methods.json'),source_hashes={str(p.relative_to(a.app_dir)):hashlib.sha256(p.read_bytes()).hexdigest()for p in a.app_dir.rglob('*')if p.is_file()})
a.output.write_text(json.dumps(payload,indent=2)+'\n')
assert all(t['returncode']==0 and t['metrics']['after_close']==0 for t in results)
