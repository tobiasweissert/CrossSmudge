assert(smudge.get_device()=='sticky')
local read=smudge.read_file;local files={};smudge.millis=function()return 123456 end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local s=files[path];return s and s:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,s)files[path]=s;return true end
local app=dofile('main.lua');on_init();on_draw()
local function box(action,value)for _,t in ipairs(app.view.hit)do if t.action==action and(value==nil or t.value==value)then return t.box end end;error('missing control '..tostring(action))end
local function tap(b,draw)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');if draw~=false then on_draw()end end
local function hit(action,value)tap(box(action,value))end
hit(2);for seat=1,4 do hit(1);hit(1)end;assert(app.state().phase==3);hit(1);assert(app.state().phase==5);hit('choose',0)
local confirm=box('accuse',0);tap(confirm,false);assert(app.state().phase==6 and app.state().rounds==1)
tap(confirm,false);assert(app.state().phase==6 and app.state().rounds==1,'queued old vote confirmation skipped the unseen result and dealt another private round')
on_draw();on_exit()
