"""Exact preserved-module/API-dispatch regression; device identity is an explicit test input."""
import argparse,hashlib,io,json,shutil,subprocess,tarfile,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2];P=argparse.ArgumentParser();P.add_argument('--output',type=Path,required=True);a=P.parse_args();a.output.mkdir(parents=True,exist_ok=True)
BASE='621719e4f68d8ddf719d470f84be20ad22c11d35';L=R/'lib/Lua/src';runner=a.output/'dispatch-runner'
subprocess.run(['cc','-O2','-I'+str(L),str(R/'test/crossplay_ports/runner.c'),*[str(p)for p in sorted(L.glob('*.c'))],'-lm','-o',str(runner)],check=True,capture_output=True)
fixture=r'''
local write=smudge.write_file;local data={};smudge.write_file=function(p,s)data[p]=s;return true end
local read=smudge.read_file;smudge.read_file=function(p,n,o)if data[p]then return data[p]:sub((o or 0)+1,(o or 0)+n)end;if p:match('%.dat$')then return nil end;return read(p,n,o)end
smudge.get_device=function()return 'DEVICE'end
local sum,count=7,0
local function hash(s)for i=1,#s do sum=(sum*31+s:byte(i))%65521 end end
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap','text','centered_text','header','button_hints'})do local old=smudge[name];smudge[name]=function(...)count=count+1;hash(name);for _,v in ipairs({...})do hash(type(v)..':'..tostring(v))end;return old(...)end end
dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();on_button('back',true);on_draw();on_exit();assert(write('dispatch.proof',sum..':'..count,false))
'''
results=[];preserved={}
with tempfile.TemporaryDirectory(dir=a.output)as temp:
 temp=Path(temp)
 archive=subprocess.check_output(['git','archive',BASE,*['apps/cp-'+x for x in ('connectfour','knucklebones','checkers')]],cwd=R)
 with tarfile.open(fileobj=io.BytesIO(archive))as tar:tar.extractall(temp/'baseline',filter='data')
 for title in ('connectfour','knucklebones','checkers'):
  current=R/'apps'/('cp-'+title);baseline=temp/'baseline/apps'/('cp-'+title)
  for stem in ('board','front','graphics','presentation','enhanced'):
   assert (current/('legacy_'+stem+'.lua')).read_bytes()==(baseline/(stem+'.lua')).read_bytes()
   preserved[f'{title}/legacy_{stem}.lua']=hashlib.sha256((current/('legacy_'+stem+'.lua')).read_bytes()).hexdigest()
  for cap in (76800,2097152):
   for width,height in ((480,800),(800,480)):
    for device in ('x4-pro','x4','simulator','older-host-error'):
     proofs=[]
     for kind,source in (('baseline',baseline),('current',current)):
      app=temp/(kind+title+str(cap)+str(width)+device);shutil.copytree(source,app)
      body=fixture.replace('DEVICE',device)
      if device=='older-host-error':body=body.replace("return 'older-host-error'","error('ABI unavailable')")
      (app/'fixture.lua').write_text(body);r=subprocess.run([str(runner.resolve()),'fixture.lua',str(cap),str(width),str(height)],cwd=app,text=True,capture_output=True)
      assert r.returncode==0,(title,kind,cap,width,device,r.stderr)
      metrics=json.loads(r.stdout.strip().splitlines()[-1]);assert metrics['after_close']==0
      proofs.append((app/'dispatch.proof').read_bytes())
      results.append(dict(title=title,kind=kind,cap=cap,width=width,device_fixture=device,metrics=metrics))
     assert proofs[0]==proofs[1],(title,cap,width,device,proofs)
(a.output/'results.json').write_text(json.dumps(dict(baseline=BASE,scope='Preserved module byte identity and matching API-call digest for actual Lua dispatch; host draw stubs, explicit device fixture; not physical non-Sticky screenshots',preserved=preserved,tests=results),indent=2)+'\n')
print('PASS',len(results),'baseline/current executions; all call digests equal')
