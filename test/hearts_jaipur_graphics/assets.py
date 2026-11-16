"""Repack original 1-bit CrossPlay font masks and Sea Salt Lucide art."""
from pathlib import Path
import argparse,hashlib,json,re,shutil
p=argparse.ArgumentParser();p.add_argument('reference',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[2]
for app in ['cp-hearts','cp-jaipur']:
 names=['toybox_10','toybox_14','toybox_20','toybox_30']
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
 for relative in (['src/apps_local/cards/CardSuits.h'] if app=='cp-hearts' else ['src/apps_local/jaipur/JaipurArt.h','src/apps_local/jaipur/JaipurGoods.h']):
  src=a.reference/relative;s=src.read_text();sources[relative]=hashlib.sha256(src.read_bytes()).hexdigest()
  for name,body in re.findall(r'icon_(\w+_\d+)_bits\[\] = \{(.*?)\};',s,re.S):
   data=bytes(int(v,16)for v in re.findall(r'0x([0-9a-fA-F]{2})',body));size=int(name.rsplit('_',1)[1]);assert len(data)==((size+7)//8)*size
   (out/(name+'.raw')).write_bytes(data)
 shutil.copyfile(a.reference/'site/assets/fonts/OFL-Jersey25.txt',root/'apps'/app/'OFL-Jersey25.txt')
 (out/'sources.json').write_text(json.dumps(sources,indent=2)+'\n')
