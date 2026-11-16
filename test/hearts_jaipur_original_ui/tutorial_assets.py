"""Exact original Jaipur fixed tutorial illustrations, native raster, 1-bit masks.
The live port renders chrome/captions/navigation and the entire eighth rules page.
"""
from pathlib import Path
import argparse,subprocess,json,hashlib
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--native',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rows=[]
for page in range(7):
 out=a.output/f'jaipur-help-{page+1}.pgm';subprocess.run([str(a.native),'jaipur-help','480','800',str(out),str(page)],check=True)
 pix=out.read_bytes().split(b'\n',3)[3];height=280;data=bytearray()
 for y in range(199,199+height):
  for bx in range(56):
   n=0
   for k in range(8):
    if pix[y*480+16+bx*8+k]==0:n|=128>>k
   data.append(n)
 dest=R/'apps/cp-jaipur/art'/f'tutorial-{page+1}.raw';data=bytes(v^255 for v in data);dest.write_bytes(data);rows.append(dict(page=page+1,x=16,y=199,w=448,h=height,path=str(dest.relative_to(R)),sha256=hashlib.sha256(data).hexdigest()))
# Inactive dot uses the original renderer's rounded DarkGray stroke.
raw=(a.output/'jaipur-help-2.pgm').read_bytes().split(b'\n',3)[3];bits=bytearray()
for y in range(14):
 for byte in range(2):
  v=0
  for k in range(8):
   x=byte*8+k
   if x<14 and raw[(770+y)*480+149+x]==0:v|=128>>k
  bits.append(v)
(Path(__file__).resolve().parents[2]/'apps/cp-jaipur/art/tutorial-dot.raw').write_bytes(bytes(v^255 for v in bits))
(a.output/'tutorial-masks.json').write_text(json.dumps(rows,indent=2)+'\n')
