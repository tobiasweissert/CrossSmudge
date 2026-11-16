assert(smudge.get_device()=='sticky')
local read=smudge.read_file;local saved={};local now=123456;local rejectUi=false
smudge.millis=function()return now end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local s=saved[path];return s and s:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,s)if rejectUi and path:match('^ui%-')then return false end;saved[path]=s;return true end
local app
local function boot()app=dofile('main.lua');on_init();on_draw()end
local function hit(action,value)
 for _,t in ipairs(app.view.hit)do if t.action==action and(value==nil or t.value==value)then local b=t.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing actual control '..tostring(action)..'/'..tostring(value))
end
local function category(n)hit('picker');hit('category',n)end
local function roundZero()
 hit(4);on_button('confirm',true);on_draw();now=now+60001;on_update();on_draw();assert(app.state().phase==3 and app.state().got==0);hit(3)
end
boot();roundZero();category(2);roundZero();on_exit()
assert(saved['ui-categories-a.dat']and saved['ui-categories-b.dat'],'setup must retain both previous UI slots')
boot();hit('settings');hit('reset');rejectUi=true;hit('reset');on_exit();assert(app.state().rounds==0)
boot();assert(app.state().rounds==0);category(3);hit(4);assert(app.state().phase==0 and app.state().rounds==0,'must block new round until category reset clear is durable');hit(4);assert(app.state().phase==0 and app.state().rounds==0);on_exit()
rejectUi=false;boot();category(3);roundZero();roundZero();assert(app.state().rounds==2);on_exit();boot();capture('resurrected-categories.pgm');local ui=dofile('played.lua');ui.init(app.state())
print('POST_RESET_UI_BITS',ui.has(1),ui.has(2),ui.has(3),'rounds',app.state().rounds)
assert(ui.has(3)and not ui.has(1)and not ui.has(2),'stale pre-reset higher-sequence sidecar resurrected old categories and hid newly observed zero')
on_exit()
