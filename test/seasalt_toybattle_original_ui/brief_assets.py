from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--native',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);R=Path(__file__).resolve().parents[2];rows=[]
for map in range(10):
 for bases in [0,1]:
  out=a.output/f'brief-{map}-{bases}.pgm';subprocess.run([str(a.native),'toybattle-brief','480','800',str(out),f'{map} {bases}'],check=True);pixels=out.read_bytes().split(b'\n',3)[3];raw=bytearray([255]*(56*665))
  for y in range(665):
   for x in range(448):
    if pixels[(y+119)*480+x+16]==0:raw[y*56+x//8]&=~(128>>(x%8))
  dest=R/f'apps/cp-toybattle/art/brief-{map}-{bases}.raw';dest.write_bytes(raw);rows.append(dict(map=map,bases=bases,path=str(dest.relative_to(R)),sha256=hashlib.sha256(raw).hexdigest()))
(a.output/'briefs.json').write_text(json.dumps(rows,indent=2)+'\n')
