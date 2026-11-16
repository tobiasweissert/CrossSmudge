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
for app in ['insider','forehead','wavelength']:
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
