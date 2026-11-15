"""Repack original 1-bit CrossPlay font masks and Sea Salt Lucide art."""
from pathlib import Path
import argparse,hashlib,json,re,shutil
p=argparse.ArgumentParser();p.add_argument('reference',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[2]
for app,names in [('cp-seasalt',['toybox_10','toybox_20','toybox_30'])]:
 out=root/'apps'/app/'art';out.mkdir(exist_ok=True);sources={}
 for name in names:
  src=a.reference/f'src/apps_local/ui/fonts/{name}.h';s=src.read_text();sources[str(src.relative_to(a.reference))]=hashlib.sha256(src.read_bytes()).hexdigest()
  bits=bytes(int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{2})',s.split('Bitmaps')[1].split('};')[0]))
  glyphs=[list(map(int,v.split(','))) for v in re.findall(r'\{([\d, -]+)\}',s.split('Glyphs[]')[1].split('};')[0])]
  intervals=[list(int(v,16) for v in m) for m in re.findall(r'\{0x([0-9a-fA-F]+), 0x([0-9a-fA-F]+), 0x([0-9a-fA-F]+)\}',s.split('Intervals[]')[1].split('};')[0])]
  bycode={cp:glyphs[start+cp-lo] for lo,hi,start in intervals for cp in range(lo,hi+1)}
  header,payload=bytearray(),bytearray()
  for cp in range(32,256):
   w,h,advance,left,top,n,offset=bycode.get(cp,bycode[63]);start=224*7+len(payload)
   header.extend([w,h,(advance+8)//16,left%256,top%256,start%256,start//256])
   stride=(w+7)//8;raw=bytearray([255]*(stride*h))
   for y in range(h):
    for x in range(w):
     bit=y*w+x
     if bits[offset+bit//8]&(128>>(bit%8)):raw[y*stride+x//8]&=~(128>>(x%8))
   payload.extend(raw)
  assert len(header+payload)<65536
  (out/(name+'.bin')).write_bytes(header+payload)
 if app=='cp-seasalt':
  for headername in ['SeaSaltArt.h','SeaSaltMarks.h']:
   src=a.reference/'src/apps_local/seasalt'/headername;s=src.read_text();sources[str(src.relative_to(a.reference))]=hashlib.sha256(src.read_bytes()).hexdigest()
   for name,body in re.findall(r'icon_(ss_\w+_\d+)_bits\[\] = \{(.*?)\};',s,re.S):
    data=bytes(int(v,16)for v in re.findall(r'0x([0-9a-fA-F]{2})',body));size=int(name.rsplit('_',1)[1]);assert len(data)==((size+7)//8)*size
    if name.startswith('ss_m_')and size!=24:continue
    (out/(name+'.raw')).write_bytes(data)
    if name.startswith('ss_m_'):
     side=20;small=bytearray([255]*(((side+7)//8)*side))
     for y in range(side):
      for x in range(side):
       sx=x*size//side;sy=y*size//side
       if not data[sy*((size+7)//8)+sx//8]&(128>>(sx%8)):small[y*((side+7)//8)+x//8]&=~(128>>(x%8))
     (out/(name.rsplit('_',1)[0]+'_20.raw')).write_bytes(small)

  license=a.reference/'freeink-sdk/libs/assets/Icons/lucide/LICENSE'
  destination=root/'apps'/app/'LICENSE-Lucide.txt'
  if license.exists():shutil.copyfile(license,destination)
  else:assert destination.is_file(),'Initialize the reference SDK or supply the retained Lucide license'
 shutil.copyfile(a.reference/'site/assets/fonts/OFL-Jersey25.txt',root/'apps'/app/'OFL-Jersey25.txt')
 (out/'sources.json').write_text(json.dumps(sources,indent=2)+'\n')
