"""Lossless original ASCII glyph masks plus differential kerning steps, OFL provenance."""
import argparse,hashlib,json,re,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--app',type=Path,required=True);a=p.parse_args();out=a.app/'art';out.mkdir(exist_ok=True)
provenance={}
for name,key in [('toybox_10','font10'),('toybox_20','font20'),('toybox_30','font30'),('instrument_10','serif10'),('instrument_13','serif13')]:
 source=a.reference/'src/apps_local/ui/fonts'/f'{name}.h';assert source.read_bytes()==subprocess.check_output(['git','show','d3a49ce8:'+str(source.relative_to(a.reference))],cwd=a.reference),'Original font changed';s=source.read_text();bits=bytes(int(v,16)for v in re.findall(r'0x([\da-fA-F]{2})',s.split('Bitmaps')[1].split('};')[0]));rows=[list(map(int,v.split(',')))for v in re.findall(r'\{([\d, -]+)\}',s.split('Glyphs[]')[1].split('};')[0])][:95]
 header=bytearray();pixels=bytearray()
 for w,h,advance,left,top,count,offset in rows:
  start=95*7+len(pixels);header.extend([w,h,(advance+8)//16,left%256,top%256,start%256,start//256]);raw=bytearray([255]*(((w+7)//8)*h))
  for y in range(h):
   for x in range(w):
    bit=y*w+x
    if bits[offset+bit//8]&(128>>(bit%8)):raw[y*((w+7)//8)+x//8]&=~(128>>(x%8))
  pixels.extend(raw)
 (out/(key+'.bin')).write_bytes(header+pixels)
 if name.startswith('instrument'):
  def classes(which):
   block=s.split(name+'Kern'+which+'Classes[]')[1].split('};')[0]
   return {int(code,16):int(value)for code,value in re.findall(r'\{0x([\dA-Fa-f]+),\s*(\d+)\}',block)}
  left,right=classes('Left'),classes('Right');matrix=list(map(int,re.findall(r'-?\d+',s.split(name+'KernMatrix[]')[1].split('{',1)[1].split('};')[0])))
  rightCount=max(right.values());steps=bytearray()
  for prev in range(32,127):
   for current in range(32,127):
    lc,rc=left.get(prev,0),right.get(current,0);kern=matrix[(lc-1)*rightCount+rc-1]if lc and rc else 0
    steps.append((rows[prev-32][2]+kern+8)//16)
  (out/(key+'-steps.bin')).write_bytes(steps)
 provenance[str(source.relative_to(a.reference))]=hashlib.sha256(source.read_bytes()).hexdigest()
(out/'sources.json').write_text(json.dumps(provenance,indent=2)+'\n')
shutil.copyfile(a.reference/'site/assets/fonts/OFL-InstrumentSerif.txt',a.app/'OFL-InstrumentSerif.txt')
