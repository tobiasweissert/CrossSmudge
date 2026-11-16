assert(smudge.get_device()=='sticky');local app=dofile('main.lua');on_init();on_draw();local V,g=app.view,app.state()
local function find(action)for i,h in ipairs(V.hit)do if h.action==action then return h,i end end;error('missing '..tostring(action))end
local function contact(h)on_touch(h.box.x+h.box.w//2,h.box.y+h.box.h//2,'tap')end
local function tap(action)local h=find(action);contact(h);on_draw()end
local function queued(action,phase,view)
 local h,i=find(action);contact(h);contact(h);V.focus=i;on_button('confirm',true)
 assert(g.phase==phase,'B12-02: stale action skipped private phase');on_draw();assert(V.currentView==view)
end
tap('mode');tap('start');tap('ready');queued(1,2,'peek');assert(not V.everRevealed and not V.clue)
on_long_press(240,245);assert(not V.peek,'old spectrum hold must not reveal');on_touch_release(240,245)
on_button('back',true);on_button('confirm',true);on_draw();assert(V.currentView=='pause','stale confirm after Back');tap('resume');assert(not V.peek)
on_long_press(240,680);on_draw();assert(V.peek);on_touch_release(240,680);on_draw();assert(not V.peek and V.everRevealed)
queued('clue',3,'clue');queued('dial',3,'dial');queued(3,4,'call');queued(1,5,'reveal');queued('next',5,'pass');tap('ready');queued(1,2,'peek');on_long_press(240,680);on_draw();on_touch_release(240,680);on_draw();tap('clue');tap('dial');tap(3);assert(g.phase==4 and g.round==1 and g.mode==2);capture('scored-call.pgm');on_exit()
