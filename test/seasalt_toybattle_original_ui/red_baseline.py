"""Reproduce the immutable pre-change pixel failure using actual old callbacks."""
from pathlib import Path
import argparse,subprocess,tempfile,json
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--runner',type=Path,required=True);a=p.parse_args();R=Path(__file__).resolve().parents[2];F=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references/captures');a.output.mkdir(parents=True,exist_ok=True);rows=[]
with tempfile.TemporaryDirectory(dir=a.output)as t:
 for app,w,h in [('seasalt',480,800),('toybattle',480,800)]:
  d=Path(t)/app;d.mkdir();rel='apps/cp-'+app
  for f in subprocess.check_output(['git','ls-tree','-r','--name-only','621719e4',rel],cwd=R,text=True).splitlines():
   dest=d/Path(f).relative_to(rel);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(subprocess.check_output(['git','show','621719e4:'+f],cwd=R))
  (d/'fixture.lua').write_text("assert(smudge.get_device()=='sticky');dofile('main.lua');on_init();on_draw();capture('menu.pgm');on_exit()")
  r=subprocess.run([str(a.runner),'fixture.lua','2097152',str(w),str(h)],cwd=d,text=True,capture_output=True,check=True)
  raw=(d/'menu.pgm').read_bytes();(a.output/(app+'-menu.pgm')).write_bytes(raw);target=(F/app/f'menu-{w}x{h}.pgm').read_bytes();pa=raw.split(b'\n',3)[3];pb=target.split(b'\n',3)[3];changed=sum(x!=y for x,y in zip(pa,pb));assert changed>0
  rows.append(dict(app=app,reference='621719e4',pixel_expected=0,actual=changed,observed_red=True,runtime=r.stdout))
(a.output/'red.json').write_text(json.dumps(rows,indent=2)+'\n')
