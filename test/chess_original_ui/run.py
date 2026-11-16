#!/usr/bin/env python3
"""Actual production raster/libs/Sticky identity and2MiB int32/float32 VM."""
from pathlib import Path
import argparse,subprocess,shutil,json,sys,hashlib,struct,zlib
R=Path(__file__).resolve().parents[2];D=Path(__file__).parent
p=argparse.ArgumentParser();p.add_argument('--work-dir',type=Path,default=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/chess-ui'));p.add_argument('--runner',type=Path);p.add_argument('--app-dir',type=Path,default=R/'apps/chess');p.add_argument('--red',action='store_true');a=p.parse_args();a.work_dir.mkdir(parents=True,exist_ok=True)
if not a.runner:
 subprocess.run([sys.executable,str(R/'test/crossplay_ui_parity/build_capture.py'),'--output',str(a.work_dir/'raster')],check=True);a.runner=a.work_dir/'raster/capture-runner'
(a.work_dir/'callbacks.lua').write_text((D/'support.lua').read_text()+'\n'+(D/'callbacks.lua').read_text());rows=[]
for w,h in[(480,800),(800,480)]:
 out=a.work_dir/'fixtures'/f'{w}x{h}';shutil.rmtree(out,ignore_errors=True);shutil.copytree(a.app_dir,out);t=subprocess.run([str(a.runner),str(a.work_dir/'callbacks.lua'),'2097152',str(w),str(h)],cwd=out,capture_output=True,text=True,timeout=120);rows.append(dict(w=w,h=h,returncode=t.returncode,stdout=t.stdout,stderr=t.stderr));print(w,t.returncode,t.stdout,t.stderr)
def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
for image in(a.work_dir/'fixtures').rglob('*.pgm'):
 _,wh,_,pix=image.read_bytes().split(b'\n',3);w,h=map(int,wh.split());out=a.work_dir/'captures'/image.relative_to(a.work_dir/'fixtures').with_suffix('.png');out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pix[y*w:(y+1)*w]for y in range(h))))+chunk(b'IEND',b''))
def pixels(path):
 data=path.read_bytes();pos=8;packed=b'';width=height=None
 while pos<len(data):
  n=struct.unpack('>I',data[pos:pos+4])[0];kind=data[pos+4:pos+8];body=data[pos+8:pos+8+n];pos+=n+12
  if kind==b'IHDR':width,height,depth,color,_,_,_=struct.unpack('>IIBBBBB',body);assert depth==8 and color==0
  if kind==b'IDAT':packed+=body
 raw=zlib.decompress(packed);assert all(raw[y*(width+1)]==0 for y in range(height));return width,height,b''.join(raw[y*(width+1)+1:(y+1)*(width+1)]for y in range(height))
meta=json.loads((D/'reference/sources.json').read_text());comparisons=[]
for ref in sorted((D/'reference').glob('*.png')):
 assert hashlib.sha256(ref.read_bytes()).hexdigest()==meta['sha256'][ref.name]
 state,dimensions=ref.stem.rsplit('-',1);w,h=map(int,dimensions.split('x'));port=a.work_dir/'fixtures'/dimensions/(state+'.pgm');assert port.exists(),port;_,wh,_,actual=port.read_bytes().split(b'\n',3);assert wh==f'{w} {h}'.encode();_,_,expected=pixels(ref);masks=[];reason=None;required=True;excluded_rows=0
 if w==800 and state not in['front','saved-front','settings-menu','settings-board','settings-restart']:
  required=False;reason='Native landscape cell28px paints48px pieces; preserve readable original24px cut and bounded landscape layout.'
 if state=='settings-board':
  masks=[(16,515,w-16,643)]if w==480 else[(w-19,119,w-16,h-68)];excluded_rows=2 if w==480 else 0;reason='Existing FIND HINT/SAVE EXIT extension rows (portrait2rows); scrollbar thumb reflects8 actual rows rather than6 in landscape.'
 if state=='settings-restart':
  masks=[(16,383,w-16,511)]if w==480 else[(778,119,784,412),(681,204,772,229),(701,270,772,295),(724,336,772,361)];excluded_rows=2 if w==480 else 0;reason='Existing2extra boardactions; in landscape6actual rows require5px scrollbar space vsoriginal4rows, shifting RHS NONE/P&P/ON by5px. Exclude only affected border/track and exact RHS glyph unions.'
 total=diff=0
 for y in range(h):
  for x in range(w):
   if any(x0<=x<x1 and y0<=y<y1 for x0,y0,x1,y1 in masks):continue
   total+=1;diff+=expected[y*w+x]!=actual[y*w+x]
 comparisons.append(dict(state=state,w=w,h=h,full_different_pixels=sum(x!=y for x,y in zip(expected,actual)),scoped_different_pixels=diff,compared_pixels=total,excluded_rectangles=masks,excluded_extra_settings_rows=excluded_rows,exact_required=required,reason=reason))
for name in meta['immutable_files']:assert hashlib.sha256((a.app_dir/name).read_bytes()).hexdigest()==meta['baseline_sha256'][name],name
payload=dict(tests=rows,pixels=comparisons,deviations=meta['deviations'],renderer='Actual production raster/extractor and exact int32float32 VM/essentiallibraries/Stickyidentity; deterministic declared file/time fixtures, no host fonts or drawing mocks; host qualification only')
(a.work_dir/'results.json').write_text(json.dumps(payload,indent=2)+'\n');assert all(t['returncode']==0 and json.loads(t['stdout'].strip().splitlines()[-1])['after_close']==0 for t in rows)
if not a.red:assert all(t['scoped_different_pixels']==0 for t in comparisons if t['exact_required']),[t for t in comparisons if t['exact_required']and t['scoped_different_pixels']]
print('PASS',sum(t['exact_required']for t in comparisons),'required native full/scoped pixel gates;',sum(not t['exact_required']for t in comparisons),'landscape board adaptations explicit')
