assert(smudge.get_device()=='sticky');local entry=smudge.get_orientation();local M=dofile('core.lua');local export=dofile('export_wave.lua');local seed
-- Find an actual deal at an endpoint after a real completed practice round.
for n=1,1000 do local g=M.new(n);M.act(g,1);M.act(g,2);M.act(g,1);M.act(g,1);M.act(g,3);M.act(g,1);M.act(g,1);if g.target==20 then seed=n;break end end;assert(seed)
for distance=0,19 do
 local g=M.new(seed);M.act(g,1);M.act(g,2);M.act(g,1);M.act(g,1);M.act(g,3);M.act(g,1);M.act(g,1);assert(g.phase==1 and g.round==1 and g.target==20)
 local S=dofile('storage.lua');S.load(M.decode);assert(S.save(M.encode(g)));local app=dofile('main.lua');on_init();on_draw()
 local function tap(action)on_draw();for _,h in ipairs(app.view.hit)do if h.action==action then local b=h.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing '..tostring(action))end
 tap(1);on_long_press(240,680);on_touch_release(240,680);tap('clue');tap('dial');g=app.state();local desired=20-distance;local l=app.view.ladder;on_touch(l.board.x+20,l.board.y+(20-desired)*l.slot+l.slot//2,'tap');on_draw();assert(g.guess==desired);tap(3);assert(g.phase==4);tap(desired==20 and 2 or 1);assert(g.phase==5 and g.round==2);assert(g.points==M.score(desired,20,g.call,2));local name='distance-'..distance;capture(name..'.pgm');export(app,name);on_exit();assert(smudge.get_orientation()==entry)
end
