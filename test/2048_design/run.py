"""Run from repository root; never builds firmware or touches the PIO cache."""
from pathlib import Path
import subprocess,sys,os
root=Path.cwd(); out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
binary=Path('/root/.hermes/cache/scratch/2048/runner')
binary.parent.mkdir(parents=True,exist_ok=True)
sources=[str(p) for p in Path('lib/Lua/src').glob('*.c') if p.name not in ('lua.c','luac.c')]
cmd=['cc','-O2','-Ilib/Lua/src','test/2048_design/runner.c',*sources,'-lm','-o',str(binary)]
log=['COMMAND: '+' '.join(cmd)]
r=subprocess.run(cmd,capture_output=True,text=True);log.append(r.stdout+r.stderr);r.check_returncode()
for script,cap in [('tests.lua',2097152),('memory.lua',2097152)]:
 for w,h in [(480,800),(800,480)]:
  cmd=[str(binary),'test/2048_design/'+script,str(cap),str(w),str(h),str(out)]
  r=subprocess.run(cmd,capture_output=True,text=True);log.extend(['COMMAND: '+' '.join(cmd),r.stdout+r.stderr])
  print(r.stdout+r.stderr);(out/'tests.log').write_text('\n'.join(log));r.check_returncode()
for w,h in [(480,800),(800,480)]:
 for device in ['missing','x3','x4','x4pro','x4-pro','simulator']:
  results=[]
  for source in ['test/2048_design/baseline.lua','apps/2048/main.lua']:
   cmd=[str(binary),'test/2048_design/legacy.lua','76800',str(w),str(h),str(out)]
   env=dict(os.environ,APP2048_SOURCE=source,APP2048_DEVICE=device)
   r=subprocess.run(cmd,env=env,capture_output=True,text=True)
   log.extend(['COMMAND: '+str((cmd,source,device)),r.stdout+r.stderr])
   (out/'tests.log').write_text('\n'.join(log));r.check_returncode()
   results.append(r.stdout.rsplit('\n',2)[0]); print(device,source,r.stdout.splitlines()[-1])
  assert results[0]==results[1], 'legacy differential trace'
# Inspect every actual glyph rectangle in non-modal production board traces.
for path in out.glob('*.trace'):
 if not any(path.name.startswith(k) for k in ['reference-','late-game-','max-values-','overflow-']): continue
 w,h=map(int,path.stem.rsplit('-',1)[1].split('x'))
 gap=7 if w>h else 9
 cell=(min(w-32,h-180)-gap*3)//4;side=cell*4+gap*3
 sx=(w-side)//2;sy=89 if w>h else (h-side)//2
 for line in path.read_text().splitlines():
  if not line.startswith('sprite '):continue
  x,y,gw,gh=map(int,line.split()[1:5])
  if sy<=y+gh/2<sy+side:
   c=round((x+gw/2-(sx+cell/2))/(cell+gap));r=round((y+gh/2-(sy+cell/2))/(cell+gap))
   tx=sx+c*(cell+gap);ty=sy+r*(cell+gap)
   assert 0<=c<4 and 0<=r<4 and tx+4<=x and x+gw<=tx+cell-4 and ty+4<=y and y+gh<=ty+cell-4,(path.name,line[:80])
message='PASS: actual glyph bounding rectangles stay within all tiles, including signed int32 extremes'
print(message);log.append(message);(out/'tests.log').write_text('\n'.join(log))
from PIL import Image,ImageOps,ImageDraw
images=[]
for p in sorted(out.glob('*.pgm')):
 im=Image.open(p);im.save(p.with_suffix('.png')); thumb=im.copy();thumb.thumbnail((300,400))
 panel=Image.new('RGB',(320,430),'#ddd');panel.paste(thumb,((320-thumb.width)//2,25));ImageDraw.Draw(panel).text((8,7),p.stem,fill='black');images.append(panel)
contact=Image.new('RGB',(320*3,430*((len(images)+2)//3)),'#eee')
for i,im in enumerate(images):contact.paste(im,((i%3)*320,(i//3)*430))
contact.save(out/'contact-sheet.png')
