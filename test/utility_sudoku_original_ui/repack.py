#!/usr/bin/env python3
"""Lossless 1-bit original cuts with unrounded 12.4 advance and ink bearings."""
from pathlib import Path
import subprocess,re,json,hashlib,struct
R=Path(__file__).resolve().parents[2];REV='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b';C=Path('/root/github/crossplay')
for app,names in [('cp-calculator',[(s,'calculator/fonts/calc_jersey_'+str(s)+'.h')for s in [20,26,28,34,44,56]]+[(14,'ui/fonts/toybox_14.h'),(30,'ui/fonts/toybox_30.h')]),('cp-notes',[(s,'ui/fonts/toybox_'+str(s)+'.h')for s in [10,14,20,30]]),('sudoku',[(s,'ui/fonts/toybox_'+str(s)+'.h')for s in [10,20,30]])]:
 out=R/'apps'/app/'art';sources={}
 for size,path in names:
  path='src/apps_local/'+path;data=subprocess.check_output(['git','show',REV+':'+path],cwd=C);t=data.decode();sources[path]=hashlib.sha256(data).hexdigest()
  bits=bytes(int(x,16)for x in re.findall(r'0x([0-9A-Fa-f]{2})',t.split('Bitmaps[')[1].split('};')[0]))
  rec=[list(map(int,x.split(',')))for x in re.findall(r'\{([\d, -]+)\}',t.split('Glyphs[]')[1].split('};')[0])]
  intervals=[tuple(int(x.strip(),16)for x in g.split(','))for g in re.findall(r'\{([^{}]+)\}',t.split('Intervals[]')[1].split('};')[0])]
  glyphs={cp:rec[o+cp-f]for f,l,o in intervals for cp in range(f,l+1)};header,pixels=bytearray(),bytearray()
  # Fixed224records; spare127..129 slots alias calculator math signs.
  for cp in range(32,256):
   source={127:215,128:247,129:8722}.get(cp,cp)if app=='cp-calculator'else cp
   w,h,a,left,top,count,offset=glyphs.get(source,[0,0,0,0,0,0,0]);start=224*8+len(pixels);assert start<65536
   header+=struct.pack('<BBHbbH',w,h,a,left,top,start);raw=bytearray([255]*((w+7)//8*h))
   for y in range(h):
    for x in range(w):
     bit=y*w+x
     if bits[offset+bit//8]&(128>>(bit%8)):raw[y*((w+7)//8)+x//8]&=~(128>>(x%8))
   pixels+=raw
  (out/('original'+str(size)+'.bin')).write_bytes(header+pixels)
 notice=subprocess.check_output(['git','show',REV+':site/assets/fonts/OFL-Jersey25.txt'],cwd=C);(R/'apps'/app/'OFL-Jersey25.txt').write_bytes(notice)
 (out/'original-sources.json').write_text(json.dumps(dict(repository='ma-r-s/crossplay',revision=REV,sources=sources,format='U+0020..00FF records <BBHbbH>, exact12.4 advance,0=ink row masks; calculator mathaliases127/128/129',sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('original*.bin')}),indent=2)+'\n')
