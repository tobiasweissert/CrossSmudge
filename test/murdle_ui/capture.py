"""Capture actual Lua bitmap commands as SVG; native preview, not e-ink evidence."""
from pathlib import Path
import os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
OUT=Path('/tmp/murdle-ui-results');OUT.mkdir(exist_ok=True)
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
local app=dofile('main.lua');on_init()
local function capture(filename)name=filename;capturing=true;on_draw();add('</svg>');capturing=false end
local function hit(kind,value)
 for _,item in ipairs(app.hits()) do if item.action==kind and (value==nil or item.value==value or type(value)=='table' and type(item.value)=='table' and table.concat(value,',')==table.concat(item.value,',')) then local b=item.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return end end
 error('Missing '..kind)
end
for _,id in ipairs({1,49}) do
 local state=app.state();local fresh=app.core.new(123,id);for k,v in pairs(fresh) do state[k]=v end
 local suffix=id==1 and '-3x3.svg' or '-4x4.svg'
 capture('menu'..suffix);hit('play');on_draw();local clueFace=false;for _,item in ipairs(app.hits()) do if item.action=='clue' then clueFace=true end end;if not clueFace then hit('face',1) end;capture('clues'..suffix);hit('clue',1);capture('clue-ticked'..suffix)
 hit('face',2);capture('grid'..suffix);hit('cell',{1,1,1});on_draw();hit('cell',{1,1,1});capture('grid-marked'..suffix)
 hit('face',3);capture('dossier'..suffix);hit('accuse');capture('accuse'..suffix)
 local p=app.core.case(state);for cat=1,p.cats do on_draw();hit('pick',{cat,p.solution[cat][p.murder]}) end
 capture('accuse-picked'..suffix);hit('submit');capture('verdict'..suffix);hit('menu');on_draw()
end
hit('settings');capture('difficulty.svg');hit('menu');on_draw();hit('help');capture('how-to.svg')
on_exit()
'''
with tempfile.TemporaryDirectory() as temp:
 work=Path(temp);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/crossplay_ports/runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
 for app in ['murdle']:
  target=OUT/app;target.mkdir(exist_ok=True);fixtureDir=work/app;shutil.copytree(ROOT/'apps'/('cp-'+app),fixtureDir)
  script=fixtureDir/'capture.lua';script.write_text(setup+prefix+fixture)
  subprocess.run([str(runner),'capture.lua','2097152'],cwd=fixtureDir,check=True)
  for path in fixtureDir.glob('*.svg'):
   shutil.copyfile(path,target/path.name);subprocess.run(['rsvg-convert',str(path),'-o',str(target/path.with_suffix('.png').name)],check=True)
print('Captured actual bitmap commands to',OUT)
