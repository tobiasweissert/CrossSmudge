local w,h=smudge.get_bounds();local orientation='portrait';local sets={}
smudge.get_orientation=function()return orientation end;smudge.set_orientation=function(o)orientation=o;sets[#sets+1]=o end
smudge.get_bounds=function()return orientation=='landscape'and 800 or 480,orientation=='landscape'and 480 or 800 end
local app=dofile('main.lua');on_init();assert(orientation=='landscape');local M=dofile('core.lua');local V,g=app.view,app.state()
local function draw()
 local before=M.encode(g);on_draw();assert(M.encode(g)==before and M.valid(g))
 for _,c in ipairs(V.cards)do assert(g.board:sub(c,c)=='1'or g.board:sub(c,c)=='0','hidden opponent hand drawn')end
 for _,c in ipairs(V.controls)do local b=c.box;assert(b.x>=0 and b.y>=0 and b.x+b.w<=800 and b.y+b.h<=480,'touch control outside screen')end
end
local function tap(op,value)
 for _,c in ipairs(V.controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return end end;error('missing target '..op)
end
draw();tap('help');for page=1,4 do V.help=page;draw()end;tap('menu');draw();tap('resume');draw()
for i=1,3 do tap('act',i);draw()end
assert(#g.selection==3);tap('act',14);draw();assert(g.phase==1)
local old=M.encode(g);V.menu=true;on_update();assert(M.encode(g)==old,'AI ran behind menu');V.menu=false
local phases={};local turns=0
while g.phase~=4 and turns<4000 do
 draw();phases[g.phase]=true;turns=turns+1
 if g.phase==0 then assert(M.commit(g,{M.passpick(g,1),M.passpick(g,2),M.passpick(g,3),M.passpick(g,4)}))
 elseif g.phase==1 then if g.turn==1 then
  local _,opts=M.view(g);local wanted=M.label(M.choose(g));local index;for i,s in ipairs(opts)do if s==wanted then index=i end end;assert(index);tap('act',index)
 else local n=#g.trick;on_update();assert(#g.trick==n+1)end
 elseif g.phase==2 or g.phase==3 then tap('act',1)end
end
assert(g.phase==4);draw();assert(phases[1]and phases[2]and phases[3]);local before=M.encode(g);on_exit();assert(orientation=='portrait');on_update();on_touch(100,100,'tap');assert(M.encode(g)==before);local S=dofile('storage.lua');assert(S.load(M.decode));print('Hearts original table/rules, all match phases, private hands, legal taps, AI pause, orientation restore and save')
