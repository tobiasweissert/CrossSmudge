assert(smudge.get_device()=='sticky')
local read=smudge.read_file;local files={};smudge.millis=function()return 123456 end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local s=files[path];return s and s:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,s)files[path]=s;return true end
local app=dofile('main.lua');on_init();on_draw()
local function box(action,value)for _,t in ipairs(app.view.hit)do if t.action==action and(value==nil or t.value==value)then return t.box end end;error('missing control')end
local function tap(b,draw)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');if draw~=false then on_draw()end end
local plus,deal=box('players',1),box(2)
tap(deal,false);assert(app.state().phase==1)
-- A bounded observer stops the stale hit's unbounded players loop before the
-- host process can hang. Every preceding transition is the actual unchanged core.
local act=app.core.act;local calls=0
app.core.act=function(...)calls=calls+1;assert(calls<30,'stale painted + hit loops core actions without ever changing player count');return act(...)end
tap(plus,false)
assert(calls==0 and app.state().phase==1 and app.state().players==4,'stale menu player action reached role state')
