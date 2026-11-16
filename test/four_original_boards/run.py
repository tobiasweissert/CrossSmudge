#!/usr/bin/env python3
import argparse,hashlib,json,shutil,subprocess,sys,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2];H=Path(__file__).resolve().parent
sys.path.insert(0,str(R/'test/crossplay_ui_parity'))
from run import png,TRACE
TRACE=r'''
assert(smudge.get_device()=='sticky','compiled Sticky identity missing')
assert(math.maxinteger==2147483647 and io==nil and package==nil and debug==nil,'wrong production VM/libraries')
local originalWrite=smudge.write_file;local calls,hash,bytes=0,7,0
local function add(s)for i=1,#s do hash=(hash*31+s:byte(i))%65521 end end
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do local fn=smudge[name]
 smudge[name]=function(...)calls=calls+1;add(name);for _,value in ipairs({...})do if type(value)=='string'then bytes=bytes+#value;add(value)else add(tostring(value))end end;return fn(...)end
end
local snapshot=capture;capture=function(path)assert(originalWrite(path..'.trace','calls='..calls..' hash='..hash..' bitmap_bytes='..bytes..'\n',false));snapshot(path);calls,hash,bytes=0,7,0 end
'''
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--tooling',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--extra-native',type=Path);p.add_argument('--app-root',type=Path,default=R/'apps');p.add_argument('--titles',nargs='+',default=['go','hex','minesweeper','yahtzee']);p.add_argument('--fixture',default='callbacks.lua');p.add_argument('--red',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
results=[];diffs=[]
for title in a.titles:
 for w,h in ((480,800),(800,480)):
  out=a.output/title/f'{w}x{h}';out.mkdir(parents=True,exist_ok=True)
  with tempfile.TemporaryDirectory(dir=a.output)as tmp:
   app=Path(tmp)/'app';shutil.copytree(a.app_root/('cp-'+title),app);[(shutil.copyfile(f,app/('fixture-'+f.name)))for f in H.glob('*.lua')]
   (app/'fixture.lua').write_text(TRACE+'\nfixtureWrite=smudge.write_file\n'+(H/a.fixture).read_text())
   r=subprocess.run([str(a.tooling.resolve()/'capture-runner'),'fixture.lua','2097152',str(w),str(h)],cwd=app,text=True,capture_output=True)
   metrics=json.loads(r.stdout.strip().splitlines()[-1])if r.stdout.strip()else{}
   if metrics.get('peak_bytes',2097153)>2097152 or metrics.get('after_close')!=0:r.returncode=1
   results.append(dict(title=title,w=w,h=h,code=r.returncode,metrics=metrics,stdout=r.stdout,stderr=r.stderr));print(title,w,r.returncode,r.stderr,flush=True)
   for image in app.glob('*.pgm'):
    shutil.copyfile(image,out/image.name);png(image,out/(image.stem+'.png'))
   for suffix in ('*.trace','*.map'):
    for image in app.glob(suffix):shutil.copyfile(image,out/image.name)
  fixtures=[('menu',title),('board',title+'-board'),*((f'help{i}',f'four-{title}-help{i}')for i in range(1,6 if title=='minesweeper' else 5 if title=='yahtzee' else 1))] if a.fixture=='callbacks.lua' else [(p.stem,f'four-{title}-menu-map'if p.stem in ('record-menu','long-menu')else f'four-{title}-map')for p in sorted(out.glob('*.map'))]
  if a.fixture=='settings.lua':fixtures=[(p.stem,f'four-{title}-settings-map')for p in sorted(out.glob('*.map'))]
  if title=='minesweeper'and a.fixture=='mines_gameplay.lua':fixtures=[(p.stem,'four-minesweeper-map')for p in sorted(out.glob('*.map'))]
  if a.fixture=='yahtzee_lessons.lua':fixtures=[(f'help{i}',f'four-yahtzee-help{i}')for i in range(1,5)]
  if a.fixture=='mines_lessons.lua':fixtures=[(f'help{i}',f'four-minesweeper-help{i}')for i in range(1,6)]
  for state,fixture in fixtures:
   ref=out/(state+'-native.pgm');native=a.extra_native if fixture.startswith('four-') else a.native
   if not native:continue
   subprocess.run([str(native.resolve()),fixture,str(w),str(h),str(ref),*([str(out/(state+'.map'))]if fixture.endswith('-map')else[])],check=True,capture_output=True);png(ref,ref.with_suffix('.png'))
   actual=out/(state+'.pgm')
   if actual.exists():
    pixels=actual.read_bytes().split(b'\n',3)[3];expected=ref.read_bytes().split(b'\n',3)[3];diff=sum(x!=y for x,y in zip(pixels,expected));diffs.append(dict(title=title,w=w,state=state,diff=diff,exact_required=w==480 and not(a.fixture=='failed_records.lua'and state=='record-menu'),deviation='record-write failure notice'if a.fixture=='failed_records.lua'and state=='record-menu'else 'bounded landscape adaptation'if w==800 else None));print(state,diff)
(a.output/'results.json').write_text(json.dumps(dict(tests=results,comparisons=diffs),indent=2)+'\n')
assert all(x['code']==0 for x in results)
if not a.red:assert all(x['diff']==0 for x in diffs if x['exact_required'])
