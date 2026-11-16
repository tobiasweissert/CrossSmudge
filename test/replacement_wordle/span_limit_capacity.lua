-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local app=dofile('main.lua');on_init();on_draw();on_update();on_exit()
assert(app.state().day==1931 and app.resultOffset()==908)
assert(#app.state().results==1024 and app.core.statsFor(app.state().results,1023).won==1024)
