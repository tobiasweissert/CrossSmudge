#!/usr/bin/env python3
"""Exact original non-Sticky legacy bytes plus actual before/after pixels."""
from pathlib import Path
import argparse,subprocess,json,hashlib,shutil
R=Path(__file__).resolve().parents[2];D=Path(__file__).parent
p=argparse.ArgumentParser();p.add_argument('--runner',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
results=[]
for app in ['calculator','notes']:
 source=R/'apps'/('cp-'+app);roots={}
 for label in ['baseline','candidate']:
  out=a.output/app/label;shutil.rmtree(out,ignore_errors=True);shutil.copytree(source,out);roots[label]=out
  if label=='baseline':
   for name in['enhanced.lua','graphics.lua']:
    raw=subprocess.check_output(['git','show','621719e4f68d8ddf719d470f84be20ad22c11d35:apps/cp-'+app+'/'+name],cwd=R)
    assert raw==(source/('legacy_'+name)).read_bytes(),'legacy byte preservation failed';(out/name).write_bytes(raw)
 for device in ['x3','x4','x4pro','simulator','unknown','missing']:
  for w,h in[(480,800),(800,480)]:
   pair=[]
   for label,out in roots.items():
    support=(D/'support.lua').read_text().split('\n',1)[1]
    script=("smudge.get_device=nil\n"if device=='missing'else"smudge.get_device=function()return '"+device+"'end\n")+support+'\n'+((D/(app+'.lua')).read_text().split('-- Every write still belongs')[0]if app=='notes'else(D/(app+'.lua')).read_text())
    (out/'fixture.lua').write_text(script);t=subprocess.run([str(a.runner),'fixture.lua','2097152',str(w),str(h)],cwd=out,capture_output=True,text=True,timeout=30);assert t.returncode==0,t.stderr
    pair.append(dict(label=label,frames={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.pgm')},metrics=json.loads(t.stdout.strip().splitlines()[-1])))
   assert pair[0]['frames']==pair[1]['frames'],(app,device,w)
   results.append(dict(app=app,device=device,w=w,h=h,pair=pair))
(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS 24 nonSticky baseline/candidate real raster pairs')
