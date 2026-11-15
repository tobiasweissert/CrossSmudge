"""Exact firmware int32/float32 Lua and bounded-heap presentation checks."""
from pathlib import Path
import json,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
results=[]
with tempfile.TemporaryDirectory(prefix='hearts-jaipur-')as tmp:
 work=Path(tmp);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(HERE/'runner.c'),*map(str,(ROOT/'lib/Lua/src').glob('*.c')),'-lm','-o',str(runner)],check=True)
 for app in ['hearts','jaipur']:
  for script,cap in [(HERE/(app+'.lua'),2097152),(HERE/'capture.lua',2097152),(HERE/'records.lua',2097152),(HERE/'compact.lua',76800),(ROOT/'test/crossplay_ports'/(app+'.lua'),76800)]:
   for w,h in [(480,800),(800,480)]:
    folder=work/'app';shutil.rmtree(folder,ignore_errors=True);shutil.copytree(ROOT/'apps'/('cp-'+app),folder)
    r=subprocess.run([str(runner),str(script),str(cap),str(w),str(h)],cwd=folder,text=True,capture_output=True,timeout=120)
    print(app,script.name,cap,w,r.returncode,r.stderr.strip(),r.stdout.strip(),flush=True)
    results.append(dict(app=app,test=script.name,cap=cap,w=w,h=h,code=r.returncode,stdout=r.stdout,stderr=r.stderr))
    if script.name=='capture.lua'and r.returncode==0:
     out=Path('/tmp/hearts-jaipur-captures');out.mkdir(exist_ok=True)
     for path in folder.glob('*.svg'):shutil.copyfile(path,out/(app+'-'+path.name))
Path('/tmp/hearts-jaipur-results.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(r['code']==0 for r in results)
