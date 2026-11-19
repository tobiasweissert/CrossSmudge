from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--native',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);R=Path(__file__).resolve().parents[2];rows=[]
for page in range(8):
 out=a.output/f'help-{page+1}.pgm';subprocess.run([str(a.native),'toybattle-help','480','800',str(out),str(page)],check=True);cap,pic=map(int,Path(str(out)+'.meta').read_text().split());pixels=out.read_bytes().split(b'\n',3)[3];raw=bytearray([255]*(56*pic))
 for y in range(pic):
  for x in range(448):
   if pixels[(y+119)*480+x+16]==0:raw[y*56+x//8]&=~(128>>(x%8))
 dest=R/f'apps/cp-toybattle/art/help-diagram-{page+1}.raw';dest.write_bytes(raw);rows.append(dict(page=page+1,caption_height=cap,picture_height=pic,path=str(dest.relative_to(R)),sha256=hashlib.sha256(raw).hexdigest()))
(a.output/'diagrams.json').write_text(json.dumps(rows,indent=2)+'\n')
