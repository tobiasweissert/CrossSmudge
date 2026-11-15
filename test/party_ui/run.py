"""Exercise party frontends using the exact vendored Lua VM and bounded allocator."""
from pathlib import Path
import json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
OUT=Path(os.environ.get('PARTY_UI_OUT','/tmp/party-ui-results'));OUT.mkdir(exist_ok=True)
records=[]
with tempfile.TemporaryDirectory() as d:
 work=Path(d);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/crossplay_ports/runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
 for app in ['forehead','insider','wavelength']:
  for script in ['behavior.lua','party_integration.lua','party_capacity.lua','fallback.lua']:
   for cap in [76800,2097152]:
    if script=='behavior.lua' and cap==76800:continue
    if script=='fallback.lua' and app!='wavelength' and cap!=76800:continue
    for width,height in [(480,800),(800,480)]:
     fixture=work/(app+'-'+script+'-'+str(cap)+'-'+str(width));shutil.copytree(ROOT/'apps'/('cp-'+app),fixture)
     test=HERE/script if script in ['behavior.lua','fallback.lua'] else ROOT/'test/crossplay_ports'/script
     r=subprocess.run([str(runner),str(test),str(cap),str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
     metrics=json.loads(r.stdout.strip().splitlines()[-1]);assert metrics['after_close']==0
     row=dict(metrics=metrics,app=app,test=script,cap=cap,width=width,height=height,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr);records.append(row)
     print(app,script,cap,width,r.returncode,r.stderr.strip(),flush=True)
(OUT/'results.json').write_text(json.dumps(records,indent=2)+'\n')
assert all(r['returncode']==0 for r in records)
