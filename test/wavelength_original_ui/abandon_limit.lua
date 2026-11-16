-- Independent reviewer RED retained in H/integration-review; expected GREEN uses65535 saturation.
local body='10|20|2,2,2,2,2|65535|fixture';local sum=7;for i=1,#body do sum=(sum*31+body:byte(i))%65521 end
assert(smudge.write_file('ui-observed-a.dat','WU1:1:'..sum..'\n'..body))
local app=dofile('main.lua');on_init();on_draw();assert(app.view.record.rounds==10 and app.view.abandoned==65535)
local function tap(action)on_draw();for _,h in ipairs(app.view.hit)do if h.action==action then local b=h.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing '..tostring(action))end
tap('start');tap('ready');tap(1);on_button('back',true);on_draw();assert(app.view.currentView=='pause');tap('abandon');assert(app.view.abandoned==65535,'abandon count must saturate without invalidating observation slots');tap('ready');tap(1)
for round=1,2 do
 on_long_press(240,680);on_touch_release(240,680);tap('clue');tap('dial');tap(3)
 if round==1 then tap('next');tap('ready');tap(1)end
end
assert(app.view.record.rounds==11);on_exit()
app=dofile('main.lua');on_init();on_draw();assert(app.view.record.rounds==11,'valid lifetime history lost after accepted65535 abandonment overflow: '..app.view.record.rounds);on_exit()
