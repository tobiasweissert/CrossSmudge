local read=smudge.read_file;local files={};local now=1000
smudge.read_file=function(path,cap)if path:match('^art/')then return read(path,cap)end;local s=files[path];return s and s:sub(1,cap)end
smudge.write_file=function(path,data,append)assert(not append);files[path]=data;return true end
smudge.millis=function()return now end
local M=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw()
local function tap(op,value)
 for _,c in ipairs(app.view.controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('No visible control: '..op)
end
tap('new');assert(app.state().ui==9);tap('act',2);assert(app.state().ui==0)
local g=app.state();assert(M.valid(g));local pair
for step=1,900 do
 if g.phase>=2 then if g.phase==3 then local n=M.new(step);for k,v in pairs(n)do g[k]=v end else M.act(g,1)end end
 if g.turn==1 and g.step==3 then M.moves(g,function(m)if m[1]==5 then pair={m[2],m[3]}end end);if pair then break end end
 assert(M.apply(g,M.ai(g)))
end
assert(pair,'no playable duo in fixture');app.view.menu=false;app.view.tab=0
for _,card in ipairs(pair)do
 on_draw();local index
 for i,t in ipairs(app.view.tiles)do if t.c==card then index=i;break end end
 assert(index);app.view.page=(index+15)//16;on_draw();tap('select',card)
end
local selected={pair[1],pair[2]};tap('duo');assert(g.place[selected[1]]==3 and g.place[selected[2]]==3,'rendered pair did not execute legal duo')
assert(M.valid(g));now=10000;on_exit();local encoded=M.encode(g)
on_update();on_touch(20,200,'tap');assert(M.encode(g)==encoded,'input changed an exited game')
app=nil;g=nil;collectgarbage('collect');app=dofile('main.lua');on_init();on_draw();assert(M.encode(app.state())==encoded,'saved board did not resume')
on_exit();print('Sea Salt real menu / two-card pair selection / journal resume / closed lifecycle passed')
