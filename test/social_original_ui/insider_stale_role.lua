local app=dofile('main.lua');on_init();on_draw()
local function target(action)for _,v in ipairs(app.view.hit)do if v.action==action then return v.box end end;error('missing control')end
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap')end
tap(target(2));on_draw();local role=target(1);tap(role);assert(app.state().phase==2 and app.state().seat==1)
tap(role);on_button('confirm',true);assert(app.state().phase==2 and app.state().seat==1,'unpainted private card advanced by queued old controls')
on_draw();tap(target(1));assert(app.state().phase==1 and app.state().seat==2);on_draw();on_exit()
print('Private role cannot advance until actually painted')
