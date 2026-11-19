#!/usr/bin/env python3
"""Production before/after pixels for every untargeted identity; no font stubs."""
from pathlib import Path
import json,subprocess,argparse,hashlib,shutil
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--work-dir',type=Path,required=True);p.add_argument('--runner',type=Path,required=True);p.add_argument('--app-dir',type=Path,default=R/'apps/cp-hackernews');a=p.parse_args()
a.work_dir.mkdir(parents=True,exist_ok=True)
metadata=json.loads((R/'test/hackernews_original_ui/reference/sources.json').read_text())
roots={}
for label in ['baseline','candidate']:
 root=a.work_dir/label;shutil.rmtree(root,ignore_errors=True);shutil.copytree(a.app_dir,root);roots[label]=root
 if label=='baseline':
  legacy=(root/'presentation.lua').read_text().split('\n',2)[2]
  assert hashlib.sha256(legacy.encode()).hexdigest()==metadata['legacy_presentation_sha256']
  (root/'presentation.lua').write_text(legacy);(root/'main.lua').write_text("return dofile('reader.lua')\n")
for name,expected in metadata['preserved_sha256'].items():assert hashlib.sha256((roots['baseline']/name).read_bytes()).hexdigest()==expected
base='''local read=smudge.read_file
smudge.read_file=function(path,n)if path:match('^art/')then return read(path,n)end end
smudge.write_file=function()return true end
smudge.get_date=function()return{year=2026,month=11,day=16,hour=12,min=0}end
smudge.time=function()return 100000 end
smudge.millis=function()return 1000 end
smudge.has_feature=function()return false end
smudge.http_cancel=function()end
local app=dofile('main.lua');on_init();on_draw();capture('fresh.pgm');on_update();on_draw();capture('unavailable.pgm');on_exit()
print('PASS original startup/unavailable/exit callbacks')
'''
records=[]
for device in ['x3','x4','x4pro','simulator','unknown','missing']:
 for width,height in [(480,800),(800,480)]:
  runs=[]
  for label,root in roots.items():
   script=("smudge.get_device=nil\n"if device=='missing'else"smudge.get_device=function()return '"+device+"'end\n")+base
   (root/'capture.lua').write_text(script)
   result=subprocess.run([str(a.runner),'capture.lua','2097152',str(width),str(height)],cwd=root,text=True,capture_output=True,timeout=30)
   assert result.returncode==0,result.stderr
   metrics=json.loads(result.stdout.strip().splitlines()[-1]);assert metrics['after_close']==0
   frames={name:hashlib.sha256((root/(name+'.pgm')).read_bytes()).hexdigest()for name in ['fresh','unavailable']}
   runs.append(dict(label=label,frames=frames,metrics=metrics))
  assert runs[0]['frames']==runs[1]['frames'],(device,width,'untargeted pixels changed')
  records.append(dict(device=device,width=width,height=height,runs=runs))
(a.work_dir/'results.json').write_text(json.dumps({'scope':'host identity fixtures, actual production raster; full before/after pixels, not physical-profile memory acceptance','pairs':records},indent=2)+'\n')
print('PASS 12untargeted identity/orientation pairs full startup/unavailable pixels unchanged')
