-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local a=dofile('main.lua');on_init();on_draw();collectgarbage('step',50)
assert(a.state().day==1931)
a.state().game.typing='APPLE';local l=a.layout()
on_touch(14+7*l.kw,l.ky+2*(l.kh+4)+2,'tap');collectgarbage('step',50);on_update();on_draw();on_exit()
assert(a.state().game.status=='Won' and a.core.statsFor(a.state().results,1931-a.resultOffset()).won==1)
