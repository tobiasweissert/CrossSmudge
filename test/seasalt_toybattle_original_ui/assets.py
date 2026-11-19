"""Generate original troop/special drawing masks from the native renderer."""
from pathlib import Path
import argparse,subprocess,json,hashlib
R=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser();p.add_argument('--native',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);rows=[]
for category,kinds,sizes in [('mark',range(1,7),[12]),('special',range(1,8),[8,9,11,12,14])]:
 for k in kinds:
  for size in sizes:
   f=a.output/f'{category}-{k}-{size}.pgm';subprocess.run([str(a.native),'toybattle-'+category,'128','128',str(f),f'{k} {size}'],check=True);pix=f.read_bytes().split(b'\n',3)[3];data=bytearray([255]*128)
   for y in range(32):
    for x in range(32):
     if pix[(y+48)*128+x+48]==0:data[y*4+x//8]&=~(128>>(x%8))
   dest=R/f'apps/cp-toybattle/art/{category}-{k}-{size}.raw';dest.write_bytes(data);rows.append({'file':str(dest.relative_to(R)),'sha256':hashlib.sha256(data).hexdigest(),'native_method':'drawTroopMark'if category=='mark'else'drawSpecialGlyph','crop':[48,48,32,32],'point':[64,64],'kind':k,'size':size})
(a.output/'masks.json').write_text(json.dumps(rows,indent=2)+'\n')
