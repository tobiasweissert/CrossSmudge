"""Capture actual Lua bitmap commands as SVG; native preview, not e-ink evidence."""
from pathlib import Path
import os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
OUT=Path(os.environ.get('PARTY_UI_OUT','/tmp/party-ui-results'));OUT.mkdir(exist_ok=True)
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
local function action(a,value)
 for _,hit in ipairs(app.view.hit) do if hit.action==a and (value==nil or hit.value==value) then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return end end
 error('No action '..tostring(a))
end
capture('menu.svg')
if app.core.title=='Forehead' then
 action('picker');capture('categories.svg');action('category',1);on_draw();action(4);capture('ready.svg');action(1);capture('card.svg')
 for n=1,14 do on_button(n%3==0 and 'down' or 'up',true) end
 app.core.tick(app.state(),app.state().left);capture('results.svg');action(3);capture('record.svg');action('help');capture('help.svg')
elseif app.core.title=='Insider' then
 action(2);capture('pass.svg');action(1);capture('role.svg')
 while app.state().phase==1 or app.state().phase==2 do on_draw();action(1) end
 capture('questions.svg');action(1);capture('vote.svg');action('choose',0);on_draw();action('accuse',0);capture('results.svg');action('menu');capture('record.svg');action('help');capture('help.svg')
else
 action(2);capture('pick.svg');action(1);capture('peek-hidden.svg')
 for _,hit in ipairs(app.view.hit) do if hit.action=='peek' then on_long_press(hit.box.x+2,hit.box.y+2);break end end
 capture('peek.svg');on_touch_release(0,0);on_draw();action('clue');capture('clue.svg');action('dial');capture('dial.svg')
 on_button('confirm',true);capture('results.svg');action(1);on_draw();action(1);on_draw()
 on_long_press(40,680);on_touch_release(0,0);on_draw();action('clue');on_draw();action('dial');on_draw();on_button('back',true);capture('pause.svg')
end
on_exit()
'''
with tempfile.TemporaryDirectory() as temp:
 work=Path(temp);runner=work/'runner'
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/crossplay_ports/runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
 for app in ['forehead','insider','wavelength']:
  target=OUT/app;target.mkdir(exist_ok=True);fixtureDir=work/app;shutil.copytree(ROOT/'apps'/('cp-'+app),fixtureDir)
  script=fixtureDir/'capture.lua';script.write_text(setup+prefix+fixture)
  subprocess.run([str(runner),'capture.lua','2097152'],cwd=fixtureDir,check=True)
  for path in fixtureDir.glob('*.svg'):
   shutil.copyfile(path,target/path.name);subprocess.run(['rsvg-convert',str(path),'-o',str(target/path.with_suffix('.png').name)],check=True)
print('Captured actual bitmap commands to',OUT)
