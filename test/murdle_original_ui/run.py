"""Real Murdle callbacks and original-model full-frame production raster gates."""
from pathlib import Path
import argparse,json,subprocess,tempfile,shutil,sys,hashlib
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];sys.path.insert(0,str(ROOT/'test/crossplay_ui_parity'))
from run import TRACE,png
from compare_battleship import read_png
p=argparse.ArgumentParser();p.add_argument('--tooling',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--app-root',type=Path,default=ROOT/'apps/cp-murdle');p.add_argument('--red',action='store_true');p.add_argument('--fixture',choices=['callbacks.lua','lifecycle.lua','stale_back.lua']);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);results=[];pixels=[]
for fixture in [a.fixture]if a.fixture else ['callbacks.lua','lifecycle.lua','stale_back.lua']:
 for w,h in [(480,800),(800,480)]:
  with tempfile.TemporaryDirectory(prefix='murdle-',dir=O)as t:
   work=Path(t)/'app';shutil.copytree(a.app_root,work);(work/'fixture.lua').write_text(TRACE+(HERE/fixture).read_text());r=subprocess.run([str(a.tooling/'capture-runner'),'fixture.lua','2097152',str(w),str(h)],cwd=work,text=True,capture_output=True,timeout=180);row=dict(fixture=fixture,entry_width=w,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr);results.append(row);dest=O/fixture/str(w);dest.mkdir(parents=True,exist_ok=True);(dest/'callbacks.log').write_text(r.stdout+r.stderr)
   if r.returncode:continue
   for state in sorted(work.glob('*.state')):
    shutil.copy(state,dest/state.name);png(state.with_suffix('.pgm'),dest/(state.stem+'.png'));shutil.copy(state.with_suffix('.pgm.trace'),dest/(state.stem+'.pgm.trace'));cw,ch,actual=read_png(dest/(state.stem+'.png'));native=dest/(state.stem+'-native.pgm');nr=subprocess.run([str(a.native),str(cw),str(ch),str(state),str(native)],text=True,capture_output=True);assert nr.returncode==0,nr.stderr;png(native,native.with_suffix('.png'));nw,nh,expected=read_png(native.with_suffix('.png'));assert(cw,ch)==(nw,nh);pixels.append(dict(state=state.stem,entry_width=w,pixel_differences=sum(x!=y for x,y in zip(actual,expected))))
report=dict(scope='Murdle mapped original builders under actual production pixel methods, int32/float32 VM and2MiB cap; not physical acceptance',tests=results,pixels=pixels,inputs={str(f.relative_to(a.app_root)):hashlib.sha256(f.read_bytes()).hexdigest()for f in a.app_root.rglob('*')if f.is_file()});(O/'results.json').write_text(json.dumps(report,indent=2)+'\n');print('cases',len(results),'failed',sum(bool(x['returncode'])for x in results),'pixelpairs',len(pixels),'nonzero',sum(bool(x['pixel_differences'])for x in pixels));assert all(x['returncode']==0 for x in results)
if not a.red:assert all(x['pixel_differences']==0 for x in pixels)
