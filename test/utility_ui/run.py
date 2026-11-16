"""Calculator/Notes actual-VM input, layout, storage and bounded-memory regressions."""
from pathlib import Path
import json,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent;OUT=Path('/tmp/utility-ui-results');OUT.mkdir(exist_ok=True)
records=[]
with tempfile.TemporaryDirectory() as d:
 work=Path(d);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/crossplay_ports/runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
 for app in ['calculator','notes']:
  for script in [app+'_integration.lua']+(['notes_capacity.lua'] if app=='notes' else [])+['behavior.lua']:
   for cap in [76800,2097152]:
    if script=='behavior.lua' and cap==76800:continue
    for w,h in [(480,800),(800,480)]:
     fixture=work/f'{app}-{script}-{cap}-{w}';shutil.copytree(ROOT/'apps'/('cp-'+app),fixture)
     source=HERE/script if script=='behavior.lua' else ROOT/'test/crossplay_ports'/script
     r=subprocess.run([str(runner),str(source),str(cap),str(w),str(h)],cwd=fixture,text=True,capture_output=True,timeout=120)
     metrics=json.loads(r.stdout.strip().splitlines()[-1]);assert metrics['after_close']==0
     records.append(dict(app=app,test=script,cap=cap,width=w,metrics=metrics,returncode=r.returncode,stderr=r.stderr));print(app,script,cap,w,r.returncode,r.stderr.strip(),flush=True)
(OUT/'results.json').write_text(json.dumps(records,indent=2)+'\n');assert all(r['returncode']==0 for r in records)
