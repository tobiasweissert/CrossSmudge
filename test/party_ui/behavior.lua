-- Tests actual frontend callbacks, original masks/hit boxes and private transitions.
local host=dofile;local text={};local full=0;local now=10000;local held=false
local width,height=smudge.get_bounds();local orientation=width>height and 'landscape' or 'portrait';local original=orientation
smudge.set_orientation=function(value)orientation=value:find('landscape',1,true) and 'landscape' or value end
smudge.get_orientation=function()return orientation end
smudge.get_bounds=function()return orientation=='landscape' and 800 or 480,orientation=='landscape' and 480 or 800 end
smudge.has_feature=function(feature)return feature=='touch_release' end
smudge.millis=function()return now end
smudge.is_button_down=function()return held end
smudge.full_refresh=function()full=full+1 end
local clear=smudge.clear
smudge.clear=function()text={};clear()end
for _,name in ipairs({'rect','rounded_rect','rect_dither'}) do
 local draw=smudge[name];smudge[name]=function(x,y,w,h,...)local sw,sh=smudge.get_bounds();assert(x>=0 and y>=0 and w>0 and h>0 and x+w<=sw and y+h<=sh,'offscreen '..name..' '..x..','..y..','..w..','..h);return draw(x,y,w,h,...)end
end
local draw=smudge.bitmap
smudge.bitmap=function(x,y,w,h,data,paper)
 local sw,sh=smudge.get_bounds();assert(x>=0 and y>=0 and x+w<=sw and y+h<=sh,'offscreen bitmap')
 assert(type(data)=='string' and #data==((w+7)//8)*h,'invalid bitmap cut');draw(x,y,w,h,data,paper)
end
dofile=function(path)
 local module=host(path)
 if path=='graphics.lua' then local write=module.text
  module.text=function(label,x,y,size,paper)text[#text+1]={label=tostring(label),size=size};return write(label,x,y,size,paper) end
 end
 return module
end
local app=dofile('main.lua');assert(app and app.view,'high-memory presentation not selected')
on_init();on_draw()
local function redraw()on_draw()end
local function state()return app.state()end
local function words()local lines={};for _,run in ipairs(text) do lines[#lines+1]=run.label end;return table.concat(lines,' ')end
local function contains(secret)assert(not words():find(secret,1,true),'private word rendered while hidden: '..secret)end
local function giant(value)
 for _,run in ipairs(text) do if run.size==64 and run.label==tostring(value) then return true end end;return false
end
local function find(action,value)
 for _,hit in ipairs(app.view.hit) do if hit.action==action and (value==nil or hit.value==value) then return hit.box end end
 error('missing visible action '..tostring(action)..' '..tostring(value))
end
local function tap(action,value)
 local b=find(action,value);on_touch(b.x+b.w//2,b.y+b.h//2,'tap');redraw()
end
local function peek()
 tap(1);assert(state().phase==2)
 assert(not giant(state().target),'secret shown before hold')
 tap('peek');assert(app.view.nudge and not app.view.peek)
 local b=find('peek');on_long_press(b.x+2,b.y+2);redraw()
 assert(app.view.peek and app.view.everRevealed and giant(state().target),'held target missing')
 local before=full;on_touch_release(0,0);redraw()
 assert(not app.view.peek and not giant(state().target) and full>before,'release retained or ghosted target')
 tap('clue');assert(state().phase==3 and app.view.clue)
 tap('dial');assert(not app.view.clue)
end
if app.core.title=='Forehead' then
 tap('picker');tap('page',2);tap('category',17);assert(state().category==17)
 tap('settings');local length=state().length;tap(3);assert(state().length~=length);tap('menu')
 tap('help');for i=1,3 do tap('help-next') end;assert(app.view.screen=='menu')
 tap(4);assert(state().phase==2 and orientation=='landscape')
 local secret=app.core.word(state().word);contains(secret);tap(1);assert(state().phase==1)
 assert(words():find(secret,1,true),'word card absent')
 for i=1,15 do on_button(i%2==0 and 'down' or 'up',true);redraw() end
 assert(state().got==8 and #state().history==75)
 now=now+state().left+1;on_update();redraw();assert(state().phase==3)
 assert(app.view.pages==2);tap('page',2);assert(state().review==13)
 tap(3);assert(state().phase==0 and orientation=='portrait' and state().rounds==1)
 tap('settings');tap('reset');assert(state().rounds==1);tap('reset');assert(state().rounds==0)
elseif app.core.title=='Insider' then
 tap('players',1);tap('players',1);tap('players',-1);assert(state().players==5)
 tap('help');for i=1,5 do tap('help-next') end;assert(app.view.help==0)
 tap(2);local secret=app.core.word(state().word)
 for seat=1,state().players do
  assert(state().phase==1 and state().seat==seat);contains(secret)
  tap(1);assert(state().phase==2)
  if state().roles:sub(seat,seat)=='C' then contains(secret) else assert(words():find(secret,1,true),'known word absent') end
  tap(1)
 end
 assert(state().phase==3,'last pass did not start clock')
 local left=state().left;now=now+5000;on_update();redraw();assert(state().left==left-5000)
 tap(1);assert(state().phase==5)
 local master=state().roles:find('M',1,true)
 for _,hit in ipairs(app.view.hit) do assert(not(hit.action=='choose' and hit.value==master),'Master is accusible') end
 tap('choose',0);assert(state().phase==5,'choice committed before confirmation')
 tap('accuse',0);assert(state().phase==6 and state().rounds==1)
 tap('menu');assert(state().phase==0 and state().word==0 and state().roles=='')
 assert(app.core.decode(app.core.encode(state())),'front door corrupted state')
else
 tap('mode');assert(state().mode==2)
 tap(2);assert(state().phase==1);peek()
 local ladder=app.view.ladder;on_touch(ladder.board.x+20,ladder.board.y+2,'tap');redraw();assert(state().guess==20)
 on_touch(ladder.board.x+20,ladder.board.y+ladder.slot//2,'tap');redraw();assert(state().guess==20,'tapping current slot moved it')
 on_button('down',true);redraw();assert(state().guess==19)
 on_button('confirm',true);redraw();assert(state().phase==4)
 tap(1);assert(state().phase==5 and state().points==0 and state().round==1)
 tap(1);peek();local target=state().target;ladder=app.view.ladder
 on_touch(ladder.board.x+20,ladder.board.y+(20-target)*ladder.slot+ladder.slot//2,'tap');redraw();assert(state().guess==target)
 on_button('back',true);redraw();assert(app.view.screen=='pause')
 tap('menu');assert(app.view.screen=='front' and state().phase==3)
 tap(2);assert(app.view.screen=='menu' and state().guess==target,'menu discarded in-flight round')
 on_button('confirm',true);redraw();tap(target==20 and 2 or 1)
 assert(state().phase==5 and state().points==5 and state().total==5)
 tap(1);tap(1);assert(state().phase==2)
 held=true;on_update();redraw();assert(app.view.peek)
 held=false;on_button('confirm',true);on_update();redraw();assert(not app.view.peek and state().phase==2,'button release leaked/advanced peek')
end
on_exit();assert(orientation==original,'exit did not restore original orientation')
assert(app.core.decode(app.core.encode(state())),'exit save failed core validation')
local serialized=app.core.encode(state());on_button('confirm',true);on_touch(100,100,'tap');assert(app.core.encode(state())==serialized,'closed input mutated state')
print('PASS',app.core.title,'original masks / real touch targets / complete phases / private transitions / saved state')
