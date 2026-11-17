#!/usr/bin/env python3
"""Actual production raster+essential libraries, exact int32/float32 2MiB VM."""
from pathlib import Path
import subprocess,json,hashlib,argparse,shutil,sys,struct,zlib
R=Path(__file__).resolve().parents[2];D=Path(__file__).parent
p=argparse.ArgumentParser();p.add_argument('--work-dir',type=Path,default=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/utility-sudoku'));p.add_argument('--runner',type=Path);p.add_argument('--test',choices=['calculator','notes','sudoku']);p.add_argument('--pixel-gate',action='store_true',help='retained compatibility; native pixel gates always run unless --red');p.add_argument('--red',action='store_true');p.add_argument('--app-root',type=Path,default=R/'apps');a=p.parse_args();H=a.work_dir.resolve();H.mkdir(parents=True,exist_ok=True)
if not a.runner:
 subprocess.run([sys.executable,str(R/'test/crossplay_ui_parity/build_capture.py'),'--output',str(H/'raster')],check=True);a.runner=H/'raster/capture-runner'
results=[]
for app in [a.test]if a.test else ['calculator','notes','sudoku']:
 for w,h in [(480,800),(800,480)]:
  fixture=H/'fixtures'/app/f'{w}x{h}';shutil.rmtree(fixture,ignore_errors=True);shutil.copytree(a.app_root/('sudoku'if app=='sudoku'else'cp-'+app),fixture)
  script=H/(app+'.lua');script.write_text((D/'support.lua').read_text()+'\n'+(D/(app+'.lua')).read_text())
  t=subprocess.run([str(a.runner),str(script),'2097152',str(w),str(h)],cwd=fixture,capture_output=True,text=True,timeout=120)
  results.append(dict(app=app,width=w,height=h,returncode=t.returncode,stdout=t.stdout,stderr=t.stderr));print(app,w,h,t.returncode,t.stdout,t.stderr)
def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
for image in(H/'fixtures').rglob('*.pgm'):
 _,wh,_,pix=image.read_bytes().split(b'\n',3);w,h=map(int,wh.split());out=H/'captures'/image.relative_to(H/'fixtures').with_suffix('.png');out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pix[y*w:(y+1)*w]for y in range(h))))+chunk(b'IEND',b''))
def pixels(path):
 data=path.read_bytes();pos=8;packed=b'';width=height=None
 while pos<len(data):
  length=struct.unpack('>I',data[pos:pos+4])[0];kind=data[pos+4:pos+8];body=data[pos+8:pos+8+length];pos+=length+12
  if kind==b'IHDR':width,height,depth,color,_,_,_=struct.unpack('>IIBBBBB',body);assert depth==8 and color==0
  if kind==b'IDAT':packed+=body
 raw=zlib.decompress(packed);assert all(raw[y*(width+1)]==0 for y in range(height));return width,height,b''.join(raw[y*(width+1)+1:(y+1)*(width+1)]for y in range(height))
meta=json.loads((D/'reference/sources.json').read_text());comparisons=[]
for ref in sorted((D/'reference').glob('*.png')):
 assert hashlib.sha256(ref.read_bytes()).hexdigest()==meta['reference_sha256'][ref.name]
 state,dimensions=ref.stem.rsplit('-',1);w,h=map(int,dimensions.split('x'));app='calculator'if state.startswith('calculator')else'notes'if state.startswith('notes')else'sudoku'
 if a.test and app!=a.test:continue
 port=H/'fixtures'/app/dimensions/(state+'.pgm');assert port.exists(),port
 _,wh,_,actual=port.read_bytes().split(b'\n',3);assert wh==f'{w} {h}'.encode();_,_,expected=pixels(ref);masks=[];required=True;reason=None
 if app=='sudoku'and w==800 and state not in['sudoku','sudoku-record']:
  required=False;reason='Original board fixes800px height; fitted landscape board and side-by-side lesson/result adaptation.'
 if app=='notes':
  if state=='notes':
   masks.append((16,h-68,w-16,h-16));reason='Native creation footer replaced with supported import help; current fixed8slots. Physical focus corners.'
   for x in[12,w-22]:
    for y in[119,221]:masks.append((x,y,x+10,y+2))
   for x in[12,w-14]:
    for y in[119,213]:masks.append((x,y,x+2,y+10))
  elif state in['notes-note','notes-check']:
   masks=[(w-8,140,w-5,h-92),(16,h-54,16+(w-44)//2,h-14)];reason='Physical focus3px rail; native ADD label becomes supported OPTIONS.'
  elif state=='notes-confirm':
   masks=[(28+(w-44)//2,h-54,w-16,h-14)];reason='Destructive label names real CLEAR DONE action, native DELETE unavailable.'
 count=diff=0;full=sum(x!=y for x,y in zip(expected,actual))
 for y in range(h):
  for x in range(w):
   if any(x0<=x<x1 and y0<=y<y1 for x0,y0,x1,y1 in masks):continue
   count+=1;diff+=expected[y*w+x]!=actual[y*w+x]
 comparisons.append(dict(state=state,w=w,h=h,full_different_pixels=full,scoped_different_pixels=diff,compared_pixels=count,excluded_rectangles=masks,exact_required=required,reason=reason))
if not a.test and not a.red:
 for name,digest in meta['preserved_sha256'].items():
  if Path(name).name=='manifest.json':
   carry=json.loads((R/'test/crosscub/catalog-preservation.json').read_text());digest=carry['apps'][Path(name).parent.name]['manifest_sha256']
  assert hashlib.sha256((a.app_root/Path(name).relative_to('apps')).read_bytes()).hexdigest()==digest,name
 assert hashlib.sha256((a.app_root/'sudoku/app.lua').read_bytes()).hexdigest()==meta['sudoku_reviewed_app_sha256']
 for app,test in [('calculator','calculator.lua'),('notes','notes.lua'),('notes','notes_capacity.lua'),('calculator','calculator_integration.lua'),('notes','notes_integration.lua')]:
  for w,h in[(480,800),(800,480)]:
   fixture=H/'supplement'/app/Path(test).stem/f'{w}x{h}';shutil.rmtree(fixture,ignore_errors=True);shutil.copytree(a.app_root/('cp-'+app),fixture);script=fixture/'fixture.lua';script.write_text((D/'support.lua').read_text()+'\n'+(R/'test/crossplay_ports'/test).read_text());t=subprocess.run([str(a.runner),str(script),'2097152',str(w),str(h)],cwd=fixture,capture_output=True,text=True,timeout=120);results.append(dict(app=app,test=test,w=w,h=h,returncode=t.returncode,stdout=t.stdout,stderr=t.stderr));print(test,w,t.returncode,t.stdout,t.stderr)
payload=dict(tests=results,pixels=comparisons,scope='Actual production raster/library/identity and exact int32/float32 VM; deterministic declared file/time API fixtures; no physical device approval',deviations=meta['deviations'])
(H/'results.json').write_text(json.dumps(payload,indent=2)+'\n')
assert all(x['returncode']==0 for x in results)
if not a.red:assert all(x['scoped_different_pixels']==0 for x in comparisons if x['exact_required']),[x for x in comparisons if x['exact_required']and x['scoped_different_pixels']]
print('PASS',sum(x['exact_required']for x in comparisons),'native full/scoped pixel gates; landscape adaptations separately reported')
