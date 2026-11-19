assert(smudge.get_device()=='sticky');local app=dofile('main.lua');on_init();on_draw()
local function tap(action)on_draw();for _,v in ipairs(app.hits())do if v.action==action then local b=v.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing '..action)end
local old=app.state().id;tap('new');assert(app.screen()=='confirm');local fresh;for _,v in ipairs(app.hits())do if v.action=='fresh'then fresh=v.box end end;assert(fresh)
on_button('back',true);assert(app.screen()=='menu');on_touch(fresh.x+fresh.w//2,fresh.y+fresh.h//2,'tap');assert(app.state().id==old,'Back canceled discard, but queued stale START A NEW ONE replaced case '..old..' with '..app.state().id);on_exit()
