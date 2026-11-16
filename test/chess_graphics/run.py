"""Chess presentation checks using the existing exact-ABI Chess runner."""
from pathlib import Path
import subprocess,tempfile,shutil,json
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
results=[]
with tempfile.TemporaryDirectory(prefix='chess-graphics-')as tmp:
 work=Path(tmp);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/replacement_chess/runner.c'),*map(str,(ROOT/'lib/Lua/src').glob('*.c')),'-lm','-o',str(runner)],check=True)
 for test in ['behavior.lua','capture.lua']:
  for w,h in [(480,800),(800,480)]:
   folder=work/'app';shutil.rmtree(folder,ignore_errors=True);shutil.copytree(ROOT/'apps/chess',folder);(folder/'legacy').mkdir()
   script=HERE/test
   r=subprocess.run([str(runner),str(script),'2097152',str(w),str(h)],cwd=folder,text=True,capture_output=True,timeout=120);print(test,w,r.returncode,r.stdout,r.stderr,flush=True)
   results.append(dict(test=test,width=w,height=h,code=r.returncode,stdout=r.stdout,stderr=r.stderr))
   if test=='capture.lua'and r.returncode==0:
    out=Path('/tmp/chess-ui-captures');out.mkdir(exist_ok=True);
    for path in folder.glob('*.svg'):shutil.copyfile(path,out/('chess-'+str(w)+'x'+str(h)+'-'+path.name))
Path('/tmp/chess-ui-results.json').write_text(json.dumps(results,indent=2)+'\n');assert all(r['code']==0 for r in results)
