"""Exact old high-heap nonSticky and constrained compact callback parity."""
from pathlib import Path
import argparse,subprocess,tempfile,shutil,json,sys
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'test/crossplay_ui_parity'));from run import TRACE
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--runner',required=True,type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);rows=[];REF='621719e4'
fixture=r'''
smudge.get_device=function()return DEVICE end
legacy_text_trace()
dofile('main.lua');on_init();on_draw();capture('entry.pgm');on_button('confirm',true);on_draw();capture('confirmed.pgm');on_exit()
'''
with tempfile.TemporaryDirectory(dir=a.output)as t:
 for app in ['seasalt','toybattle']:
  rel='apps/cp-'+app;files=subprocess.check_output(['git','ls-tree','-r','--name-only',REF,rel],cwd=R,text=True).splitlines()
  for device,limit in [('x4-pro',2097152),('x3',76800)]:
   for w,h in [(480,800),(800,480)]:
    runs=[];outputs=[]
    for variant in ['baseline','current']:
     d=Path(t)/f'{app}-{device}-{w}-{variant}';d.mkdir()
     if variant=='current':shutil.copytree(R/rel,d,dirs_exist_ok=True)
     else:
      for f in files:
       dest=d/Path(f).relative_to(rel);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(subprocess.check_output(['git','show',REF+':'+f],cwd=R))
     (d/'fixture.lua').write_text((TRACE if limit>76800 else '')+'\nDEVICE='+repr(device)+'\n'+fixture)
     r=subprocess.run([str(a.runner),'fixture.lua',str(limit),str(w),str(h)],cwd=d,text=True,capture_output=True,timeout=60);runs.append(dict(returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));outputs.append({f.name:f.read_bytes()for f in d.iterdir()if f.suffix in ['.pgm','.trace','.dat']})
    equal=runs[0]['returncode']==runs[1]['returncode']==0 and outputs[0]==outputs[1];row=dict(app=app,device=device,w=w,h=h,equal=equal,runs=runs,files=list(outputs[0]));rows.append(row);print(app,device,w,equal,flush=True)
(a.output/'preservation.json').write_text(json.dumps(rows,indent=2)+'\n');assert all(x['equal']for x in rows)
