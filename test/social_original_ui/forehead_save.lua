local read=smudge.read_file;local saved={};local now=123456;local rejectCore=false;local rejectUi=false
smudge.millis=function()return now end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local raw=saved[path];return raw and raw:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,data)if rejectCore and path:match('^state%-')or rejectUi and path:match('^ui%-')then return false end;saved[path]=data;return true end
local app
local function boot()app=dofile('main.lua');on_init();on_draw()end
local function hit(action)
 for _,target in ipairs(app.view.hit)do if target.action==action then local b=target.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing Forehead action '..action)
end
boot();hit(4);on_button('confirm',true);on_draw();now=now+60001;on_update();on_draw();assert(app.state().got==0 and app.state().rounds==1);hit(3);on_exit()
local old={};for path,raw in pairs(saved)do old[path]=raw end
boot();assert(app.state().rounds==1);hit('settings')
local box;for _,target in ipairs(app.view.hit)do if target.action=='reset'then box=target.box end end
assert(box);on_touch(box.x+3,box.y+3,'tap');on_touch(box.x+3,box.y+3,'tap');assert(app.state().rounds==1,'queued reset contact wiped unseen confirmation')
on_draw();rejectCore=true;on_touch(box.x+3,box.y+3,'tap');on_draw();assert(app.state().rounds==0);on_exit()
for path,raw in pairs(old)do assert(saved[path]==raw,'failed game write modified durable record')end
rejectCore=false;boot();assert(app.state().rounds==1 and app.state().got==0,'failed reset lost durable zero round');hit('settings');hit('reset');rejectUi=true;hit('reset');on_update();on_exit()
-- The stale UI bit is discarded when the core confirms the reset epoch.
rejectUi=false;boot();assert(app.state().rounds==0);local ui=dofile('played.lua');ui.init(app.state());assert(not ui.has(1),'failed UI clear resurrected a reset category bit');on_exit()
print('Zero result/reset contact fence/core-write failure/UI-write failure/epoch/reopen all passed')
