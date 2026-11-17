#!/usr/bin/env python3
"""Behavioral negative controls against scratch copies, never source-token proof."""
import pathlib,os,shutil,subprocess,json,resource,tempfile
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
R=pathlib.Path(__file__).resolve().parents[2]
T=pathlib.Path(os.environ.get('ASSISTED_TEST_OUT',str(pathlib.Path(tempfile.gettempdir())/'crosssmudge-browser-assisted-tests')))
O=pathlib.Path(os.environ.get('ASSISTED_MUTATION_OUT',str(T.parent/'mutations')));O.mkdir(exist_ok=True)
env=dict(os.environ,ASSISTED_PNG_FIXTURE=str(T/'palette.png'));receipts=[]
for name,file,old,new,test in [
 ('duplicate-json','json.lua',"assert(t[k]==nil,'JSON duplicate');",'', 'protocol.lua'),
 ('uncertain-gc','b7cache.lua','  if self.uncertain then return end\n  local live=roots();','  local live=roots();','storage.lua'),
 ('form-get','protocol.lua',"out.form={method='get',url=f.action,fields={}}","out.form={action=f.action,fields={}}",'client.lua')]:
 root=O/name;app=root/'apps/browser';app.mkdir(parents=True,exist_ok=True)
 for source in (R/'apps/browser').iterdir():
  if source.is_file():shutil.copyfile(source,app/source.name)
 p=app/file;s=p.read_text();assert old in s;p.write_text(s.replace(old,new,1))
 result=subprocess.run([str(T/'lua'),str(R/'test/browser_assisted'/test)],cwd=root,env=env,capture_output=True,text=True)
 (root/'result.log').write_text(result.stdout+result.stderr);receipts.append({'mutation':name,'exit':result.returncode,'detected':result.returncode!=0})
flags=['-std=c++20','-DBOARD_HAS_PSRAM=1','-I'+str(R/'lib/LuaHttp'),'-I'+str(R/'lib/LuaBrowser'),'-I'+str(R/'lib/Memory'),'-I'+str(R/'lib/miniz/src')]
source=(R/'lib/LuaHttp/HttpCore.cpp').read_text();needle='if(!q.browser || q.backend)fail("redirect_denied")';assert needle in source
p=O/'HttpCore-mutant.cpp';p.write_text(source.replace(needle,'if(!q.browser)fail("redirect_denied")',1))
subprocess.run(['c++',*flags,str(R/'test/browser_assisted/lane.cpp'),str(p),str(R/'lib/LuaHttp/HttpGzip.cpp'),str(T/'miniz.o'),'-o',str(O/'lane-mutant')],check=True)
r=subprocess.run([str(O/'lane-mutant')],capture_output=True,text=True);(O/'lane-mutant.log').write_text(r.stdout+r.stderr);receipts.append({'mutation':'backend-redirect-enabled','exit':r.returncode,'detected':r.returncode!=0})
(O/'receipt.json').write_text(json.dumps(receipts,indent=2)+'\n');assert all(r['detected']for r in receipts);print('PASS four behavioral negative controls: JSON duplicates, uncertain GC, typed forms, redirect refusal')
