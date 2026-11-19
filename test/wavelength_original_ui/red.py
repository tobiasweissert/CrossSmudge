from pathlib import Path
import argparse,subprocess,json,tempfile,sys,hashlib
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'test/crossplay_ui_parity'));from run import TRACE,png
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--runner',type=Path,required=True);p.add_argument('--native',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);rows=[];ref='621719e4';prefix='apps/cp-wavelength';inputs={};files=subprocess.check_output(['git','ls-tree','-r','--name-only',ref,prefix],cwd=R,text=True).splitlines()
for w,h in [(480,800),(800,480)]:
 with tempfile.TemporaryDirectory(dir=a.output)as t:
  d=Path(t)
  for path in files:
   dest=d/Path(path).relative_to(prefix);dest.parent.mkdir(parents=True,exist_ok=True);data=subprocess.check_output(['git','show',ref+':'+path],cwd=R);dest.write_bytes(data);inputs[path]=hashlib.sha256(data).hexdigest()
  (d/'fixture.lua').write_text(TRACE+"dofile('main.lua');on_init();on_draw();capture('before.pgm');on_exit()\n");r=subprocess.run([str(a.runner),'fixture.lua','2097152',str(w),str(h)],cwd=d,capture_output=True,text=True);assert r.returncode==0,r.stderr
  out=a.output/str(w);out.mkdir(exist_ok=True);png(d/'before.pgm',out/'before.png');(out/'before.pgm').write_bytes((d/'before.pgm').read_bytes());(out/'before.pgm.trace').write_bytes((d/'before.pgm.trace').read_bytes());subprocess.run([str(a.native),'menu','480','800',str(out/'reference.pgm')],check=True);png(out/'reference.pgm',out/'reference.png');before=(out/'before.pgm').read_bytes().split(b'\n',3)[3];target=(out/'reference.pgm').read_bytes().split(b'\n',3)[3];changed=sum(x!=y for x,y in zip(before,target));assert changed==57875
  rows.append(dict(entry_width=w,entry_height=h,expected_changed=0,observed_changed=changed,observed_red=True,stdout=r.stdout))
(a.output/'red.json').write_text(json.dumps({'baseline':ref,'inputs':inputs,'results':rows},indent=2)+'\n')
