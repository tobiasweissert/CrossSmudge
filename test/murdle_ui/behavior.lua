local w,h=480,800;local iw,ih=smudge.get_bounds();local original=iw>ih and 'landscape' or 'portrait';local orientation=original
smudge.get_orientation=function()return orientation end
smudge.set_orientation=function(o)orientation=o;w,h=o=='portrait' and 480 or 800,o=='portrait' and 800 or 480 end
smudge.get_bounds=function()return w,h end
local oldRect,oldBitmap,oldRound=smudge.rect,smudge.bitmap,smudge.rounded_rect
local function bounds(x,y,bw,bh)assert(x>=0 and y>=0 and bw>0 and bh>0 and x+bw<=w and y+bh<=h,table.concat({x,y,bw,bh,w,h},','))end
smudge.rect=function(x,y,bw,bh,...)bounds(x,y,bw,bh);return oldRect(x,y,bw,bh,...)end
smudge.rounded_rect=function(x,y,bw,bh,...)bounds(x,y,bw,bh);return oldRound(x,y,bw,bh,...)end
smudge.bitmap=function(x,y,bw,bh,data,...)bounds(x,y,bw,bh);assert(#data==((bw+7)//8)*bh);return oldBitmap(x,y,bw,bh,data,...)end
local app=dofile('main.lua');assert(app);on_init();on_draw();assert(orientation=='portrait')
local function touch(action,value)
 for _,item in ipairs(app.hits()) do
  local match=value==nil or item.value==value or type(value)=='table' and type(item.value)=='table' and table.concat(value,',')==table.concat(item.value,',')
  if item.action==action and match then local b=item.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end
 end
 error('Missing hit '..action)
end
for id=1,64 do
 local state=app.state();local fresh=app.core.new(123,id);for key,value in pairs(fresh) do state[key]=value end
 on_draw();touch('play');touch('face',2)
 local p=app.core.case(state)
 local cells=0;for _,item in ipairs(app.hits()) do if item.action=='cell' then cells=cells+1 end end
 assert(cells==#state.marks)
 touch('cell',{1,1,1});assert(state.marks[1]==1)
 touch('cell',{1,1,1});assert(state.marks[1]==2);assert(app.core.shown(state,1,1,2)==1)
 local before=state.marks[p.n+1];touch('cell',{1,2,1});assert(state.marks[p.n+1]==before) -- conflicting match refused.
 touch('cell',{1,1,1});assert(state.marks[1]==0);assert(app.core.shown(state,1,1,2)==0)
 touch('face',1);touch('clue',1);assert(app.struck()&1~=0);touch('clue',1);assert(app.struck()&1==0)
 for i=1,8 do on_swipe('up');on_draw() end
 touch('face',3);for i=1,8 do on_swipe('up');on_draw() end
 touch('accuse')
 for _,item in ipairs(app.hits()) do assert(item.action~='submit') end
 for cat=1,p.cats do touch('pick',{cat,p.solution[cat][p.murder]}) end
 touch('submit');assert(state.phase==1);assert(app.screen()=='verdict');assert(state.attempts==1)
 touch('menu');assert(app.core.decode(app.core.encode(state)))
 if id<64 then touch('new');touch('menu');assert(app.state()==state) end
 -- Core picks persist, but the UI requires an explicit selection for every category each new case.
 -- Reset UI-only reveal/pick state through a real new-case action.
 touch('new');touch('fresh');on_button('back',true);on_draw()
end
touch('settings');touch('tier',4);touch('menu');local old=app.state().id;touch('new');touch('menu');assert(app.state().id==old)
touch('new');touch('fresh');assert(app.state().id>=49);on_button('back',true);on_draw();touch('help');touch('menu')
on_exit();assert(orientation==original);assert(app.core.decode(app.core.encode(app.state())))
local saved=app.core.encode(app.state());on_touch(20,200,'tap');on_button('confirm',true);assert(app.core.encode(app.state())==saved)
print('all 64 original-grid/clue/dossier/accusation/confirmation flows OK')
