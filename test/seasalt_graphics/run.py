"""Exact int32/float32 Lua checks for Sea Salt's original screen port."""
from pathlib import Path
import json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='seasalt-ui-',dir=os.environ.get('TMPDIR','/tmp')) as tmp:
 work=Path(tmp);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(HERE/'runner.c'),*map(str,(ROOT/'lib/Lua/src').glob('*.c')),'-lm','-o',str(runner)],check=True)
 results=[]
 for script,cap in [(HERE/'records.lua',2097152),(HERE/'capture.lua',2097152),(HERE/'behavior.lua',2097152),(HERE/'integration.lua',2097152),(ROOT/'test/crossplay_ports/review_five_seasalt_capacity.lua',76800),(ROOT/'test/crossplay_ports/review_five_seasalt_capacity.lua',2097152),(ROOT/'test/crossplay_ports/five_ui.lua',76800),(ROOT/'test/crossplay_ports/five_ui.lua',2097152)]:
  for w,h in [(480,800),(800,480)]:
   app=work/'app';shutil.rmtree(app,ignore_errors=True);shutil.copytree(ROOT/'apps/cp-seasalt',app)
   r=subprocess.run([str(runner),str(script),str(cap),str(w),str(h)],cwd=app,text=True,capture_output=True,timeout=120)
   if script.name=='capture.lua'and r.returncode==0:
    out=Path('/tmp/seasalt-ui-captures');out.mkdir(exist_ok=True)
    for image in app.glob('seasalt-*.svg'):shutil.copyfile(image,out/(image.stem+'-'+str(w)+'x'+str(h)+'.svg'))
   print(script.name,cap,w,r.returncode,r.stderr.strip(),r.stdout.strip())
   results.append(dict(test=script.name,cap=cap,width=w,height=h,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
 (Path('/tmp/seasalt-graphics-results.json')).write_text(json.dumps(results,indent=2)+'\n')
 assert all(r['returncode']==0 for r in results)
