"""Compare production pixels and save bytes on unchanged non-Sticky frontends."""
from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile,shutil,sys
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'test/crossplay_ui_parity'));from run import TRACE
p=argparse.ArgumentParser();p.add_argument('--tooling',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--apps-root',type=Path,default=R/'apps');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);results=[]
fixture=r'''
local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local raw=saved[path];return raw and raw:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,data)saved[path]=data;return true end
local app=dofile('main.lua');on_init();on_draw();capture('before.pgm');on_button('confirm',true);on_draw();capture('after.pgm');on_button('back',true);on_draw();on_exit()
local paths={};for path in pairs(saved)do paths[#paths+1]=path end;table.sort(paths)
local bytes={};for _,path in ipairs(paths)do bytes[#bytes+1]=path..'\n'..saved[path]end
assert(write('saves.txt',table.concat(bytes,'\n'),false))
'''
for app in ['murdle']:
 for w,h in [(480,800),(800,480)]:
  for device in ['x4pro','unknown','missing']:
   runs=[]
   with tempfile.TemporaryDirectory(dir=O)as t:
    for label in ['baseline','candidate']:
     work=Path(t)/label;work.mkdir()
     if label=='baseline':subprocess.run(['tar','-x','--strip-components=2','-C',str(work)],input=subprocess.check_output(['git','archive','621719e4','apps/cp-'+app],cwd=R),check=True)
     else:shutil.copytree(a.apps_root/('cp-'+app),work,dirs_exist_ok=True)
     identity="smudge.get_device="+("nil"if device=='missing'else"function()return '"+device+"'end")+'\n'
     (work/'fixture.lua').write_text(TRACE+identity+fixture);r=subprocess.run([str(a.tooling/'capture-runner'),'fixture.lua','2097152',str(w),str(h)],cwd=work,capture_output=True,text=True);assert r.returncode==0,(app,device,label,r.stdout,r.stderr)
     metrics=json.loads(r.stdout.splitlines()[-1]);assert metrics['after_close']==0
     runs.append({name:hashlib.sha256((work/name).read_bytes()).hexdigest()for name in ['before.pgm','before.pgm.trace','after.pgm','after.pgm.trace','saves.txt']})
    assert runs[0]==runs[1],(app,device,w,runs)
   results.append({'app':app,'width':w,'height':h,'device':device,'exact_pixels_traces_saves':runs[0]})
(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');print('non-Sticky pairs',len(results),'all equal')

# Existing behavioral/rule/save cases use the separately labeled native API
# adapter, including the exact C3 allocator cap. No host font pixel claim.
runner=O/'legacy-adapter';subprocess.run(['cc','-O2','-I'+str(R/'lib/Lua/src'),str(R/'test/crossplay_ports/runner.c'),*[str(f)for f in sorted((R/'lib/Lua/src').glob('*.c'))],'-lm','-o',str(runner)],check=True)
legacy=[]
for fixture in ['behavior.lua','five_ui.lua','murdle.lua']:
 for cap in [76800,2097152]:
  if fixture=='behavior.lua'and cap==76800:continue
  for w,h in [(480,800),(800,480)]:
   with tempfile.TemporaryDirectory(dir=O)as t:
    work=Path(t);shutil.copytree(a.apps_root/'cp-murdle',work,dirs_exist_ok=True);source=R/('test/murdle_ui'if fixture=='behavior.lua'else'test/crossplay_ports')/fixture;r=subprocess.run([str(runner),str(source),str(cap),str(w),str(h)],cwd=work,capture_output=True,text=True,timeout=120)
    legacy.append(dict(fixture=fixture,cap=cap,width=w,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
(O/'legacy-results.json').write_text(json.dumps(legacy,indent=2)+'\n');assert all(r['returncode']==0 for r in legacy),legacy
protected=['main.lua','core.lua','storage.lua','util.lua','compact.lua','ui.lua','icon.raw','manifest.json',*['data/'+str(i)+'.lua'for i in range(1,65)]]
for name in protected:assert(a.apps_root/'cp-murdle'/name).read_bytes()==subprocess.check_output(['git','show','621719e4:apps/cp-murdle/'+name],cwd=R),name
for name in ['graphics','enhanced']:assert(a.apps_root/'cp-murdle'/('legacy_'+name+'.lua')).read_bytes()==subprocess.check_output(['git','show','621719e4:apps/cp-murdle/'+name+'.lua'],cwd=R)
print('Murdle10legacy behavioral/rule/save gates and protected baseline members green')
