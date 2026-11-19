"""Capture actual Lua bitmap commands as SVG; native preview, not e-ink evidence."""
from pathlib import Path
import os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
OUT=Path('/tmp/utility-ui-results');OUT.mkdir(exist_ok=True)
prefix=(ROOT/'test/replacement_wordle/render.lua').read_text().split('smudge.get_date=')[0]
prefix=prefix.replace('smudge.clear=function()','smudge.clear=function()\n width,height=smudge.get_bounds()')
setup='''
local name='menu.svg';local capturing=false;local nativeWrite=smudge.write_file
smudge.write_file=function(path,data,append)if path=='render.svg' and not capturing then return true end;return nativeWrite(path=='render.svg' and name or path,data,append) end
local orientation='portrait'
smudge.get_orientation=function()return orientation end
smudge.set_orientation=function(value)orientation=value:find('landscape',1,true) and 'landscape' or value end
smudge.get_bounds=function()return orientation=='landscape' and 800 or 480,orientation=='landscape' and 480 or 800 end
smudge.has_feature=function(feature)return feature=='touch_release' end
smudge.is_button_down=function()return false end
'''
fixture='''
smudge.write_file('note1.md','Groceries\\n- [ ] Milk\\n- [x] Eggs\\n- [ ] Bread for the weekend\\n- [ ] Fresh vegetables\\n')
local app=dofile('main.lua');on_init()
local function capture(filename)name=filename;capturing=true;on_draw();add('</svg>');capturing=false end
local function hit(kind,value)
 for _,item in ipairs(app.hits()) do if item.action==kind and (value==nil or item.value==value) then local b=item.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return end end
 error('Missing '..kind)
end
local function key(i)local b=app.cells()[i];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
if app.cells then
 capture('pad.svg');key(5);key(6);key(16);key(7);capture('pending.svg');key(20);capture('result.svg')
 on_long_press(100,100);capture('menu.svg')
else
 capture('library.svg');hit('library',1);capture('checklist.svg');hit('menu');capture('menu.svg')
 hit('option',2);capture('confirm.svg');hit('confirm',1);on_draw();hit('menu');on_draw();hit('option',6);capture('help.svg')
end
on_exit()
'''
with tempfile.TemporaryDirectory() as temp:
 work=Path(temp);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/crossplay_ports/runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
 for app in ['calculator','notes']:
  target=OUT/app;target.mkdir(exist_ok=True);fixtureDir=work/app;shutil.copytree(ROOT/'apps'/('cp-'+app),fixtureDir)
  script=fixtureDir/'capture.lua';script.write_text(setup+prefix+fixture)
  subprocess.run([str(runner),'capture.lua','2097152'],cwd=fixtureDir,check=True)
  for path in fixtureDir.glob('*.svg'):
   shutil.copyfile(path,target/path.name);subprocess.run(['rsvg-convert',str(path),'-o',str(target/path.with_suffix('.png').name)],check=True)
print('Captured actual bitmap commands to',OUT)
