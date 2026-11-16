#!/usr/bin/env python3
"""Lossless immutable CrossPlay masks, fixed-point advances and serif kerning."""
from pathlib import Path
import subprocess,re,json,hashlib,struct,argparse
p=argparse.ArgumentParser();p.add_argument('--app-dir',type=Path,default=Path(__file__).resolve().parents[2]/'apps/cp-hackernews');a=p.parse_args()
REF=Path('/root/github/crossplay');REV='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b'
def blob(path):return subprocess.check_output(['git','show',REV+':'+path],cwd=REF)
out=a.app_dir/'art';out.mkdir(parents=True,exist_ok=True);sources={}
for name in ['toybox_10','toybox_30','reading_serif_14','reading_serif_11','reading_serif_bold_16']:
 path='src/apps_local/ui/fonts/'+name+'.h';data=blob(path);text=data.decode();sources[path]=hashlib.sha256(data).hexdigest()
 bits=bytes(int(x,16)for x in re.findall(r'0x([0-9A-Fa-f]{2})',text.split('Bitmaps[')[1].split('};')[0]))
 records=[list(map(int,x.split(',')))for x in re.findall(r'\{([\d, -]+)\}',text.split('Glyphs[]')[1].split('};')[0])]
 intervals=[tuple(int(x.strip(),16)for x in group.split(','))for group in re.findall(r'\{([^{}]+)\}',text.split('Intervals[]')[1].split('};')[0])]
 glyphs={cp:records[offset+cp-first]for first,last,offset in intervals for cp in range(first,last+1)}
 header,pixels=bytearray(),bytearray()
 for cp in range(32,256):
  width,height,advance,left,top,count,offset=glyphs.get(cp,[0,0,0,0,0,0,0]);start=224*8+len(pixels)
  assert start<65536 and -128<=left<=127 and -128<=top<=127
  header.extend(struct.pack('<BBHbbH',width,height,advance,left,top,start))
  raw=bytearray([255]*((width+7)//8*height))
  for y in range(height):
   for x in range(width):
    bit=y*width+x
    if bits[offset+bit//8]&(128>>(bit%8)):raw[y*((width+7)//8)+x//8]&=~(128>>(x%8))
  pixels.extend(raw)
 (out/(name+'.bin')).write_bytes(header+pixels)
 if name.startswith('reading_serif'):
  left,right=bytearray(256),bytearray(256)
  for target,label in [(left,'KernLeftClasses'),(right,'KernRightClasses')]:
   for cp,cl in re.findall(r'\{(0x[0-9a-fA-F]+),\s*(\d+)\}',text.split(label+'[]')[1].split('};')[0]):target[int(cp,16)]=int(cl)
  matrix=[int(x)for x in re.findall(r'-?\d+',text.split('KernMatrix[')[1].split('{')[1].split('};')[0])]
  assert len(matrix)==36*30
  (out/(name+'-kern.bin')).write_bytes(left+right+bytes(x%256 for x in matrix))
for source,target in [('site/assets/fonts/OFL-Jersey25.txt','OFL-Jersey25.txt'),('lib/EpdFont/builtinFonts/source/NotoSerif/OFL.txt','OFL-NotoSerif.txt')]:
 data=blob(source);(a.app_dir/target).write_bytes(data);sources[source]=hashlib.sha256(data).hexdigest()
(out/'sources.json').write_text(json.dumps({'repository':'ma-r-s/crossplay','revision':REV,'sources':sources,'format':'224 U+0020..00FF records <BBHbbH>; 12.4 advances; unchanged row masks with 0=ink; serif kerning left/right maps256 plus signed36x30 matrix','output_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.bin')}},indent=2)+'\n')
