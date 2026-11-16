"""Repack original CrossPlay Jersey masks and game art without resampling."""
from pathlib import Path
import hashlib,json,re,shutil
ROOT=Path(__file__).resolve().parents[2]
REF=ROOT.parent/'CrossPlay-reference'
for app in ['forehead','insider','wavelength']:
 target=ROOT/'apps'/('cp-'+app);art=target/'art';art.mkdir(exist_ok=True);sources={}
 for size in [10,14,20,30,44,64]:
  p=REF/f'src/apps_local/ui/fonts/toybox_{size}.h';s=p.read_text();sources[str(p.relative_to(REF))]=hashlib.sha256(p.read_bytes()).hexdigest()
  bits=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s.split('Bitmaps')[1].split('};')[0]))
  glyphs=[list(map(int,x.split(','))) for x in re.findall(r'\{([\d, -]+)\}',s.split('Glyphs[]')[1].split('};')[0])][:59]
  header,payload=bytearray(),bytearray()
  for w,h,advance,left,top,n,offset in glyphs:
   start=len(glyphs)*7+len(payload);header.extend([w,h,(advance+8)//16,left%256,top,start%256,start//256])
   raw=bytearray([255]*(((w+7)//8)*h))
   for y in range(h):
    for x in range(w):
     bit=y*w+x
     if bits[offset+bit//8]&(128>>(bit%8)):raw[y*((w+7)//8)+x//8]&=~(128>>(x%8))
   payload.extend(raw)
  assert len(header+payload)<65536
  (art/f'font{size}.bin').write_bytes(header+payload)
 if app=='forehead':
  p=REF/'src/apps_local/ui/ToyboxIcons.h';s=p.read_text();names=['animals','act','sound','food','movies','people','music','jobs','sports','house','places','nature','science','myths','story','kids','tricky']
  for n,name in enumerate(names,1):
   raw=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s.split('icon_cat_'+name+'_32_bits[]')[1].split('};')[0]));assert len(raw)==128;(art/f'category{n}.raw').write_bytes(raw)
  sources[str(p.relative_to(REF))]=hashlib.sha256(p.read_bytes()).hexdigest()
 if app=='insider':
  p=REF/'src/apps_local/insider/InsiderArt.h';s=p.read_text()
  for name in ['Insider','Citizen','Master']:
   raw=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s.split('icon_role'+name+'_96_bits[]')[1].split('};')[0]));assert len(raw)==1152;(art/('role'+name+'.raw')).write_bytes(raw)
   for side in [48,56]:
    scaled=bytearray([255]*(((side+7)//8)*side))
    for y in range(side):
     for x in range(side):
      sx,sy=x*96//side,y*96//side
      if not raw[sy*12+sx//8]&(128>>(sx%8)):scaled[y*((side+7)//8)+x//8]&=~(128>>(x%8))
    (art/('role'+name+str(side)+'.raw')).write_bytes(scaled)
  sources[str(p.relative_to(REF))]=hashlib.sha256(p.read_bytes()).hexdigest()
 if app=='wavelength':
  p=REF/'src/apps_local/ui/ToyboxIcons.h';s=p.read_text()
  for name,side in [('pass',96),('look',120)]:
   raw=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s.split('icon_wl_'+name+'_'+str(side)+'_bits[]')[1].split('};')[0]));assert len(raw)==((side+7)//8)*side;(art/(name+'.raw')).write_bytes(raw)
  sources[str(p.relative_to(REF))]=hashlib.sha256(p.read_bytes()).hexdigest()
 shutil.copyfile(REF/'site/assets/fonts/OFL-Jersey25.txt',target/'OFL-Jersey25.txt')
 if app in ['forehead','insider','wavelength']:shutil.copyfile(ROOT/'apps/cp-minesweeper/LICENSE-Lucide.txt',target/'LICENSE-Lucide.txt')
 (art/'sources.json').write_text(json.dumps(sources,indent=2)+'\n')
 print(app,sum(p.stat().st_size for p in art.iterdir()),'bytes')
