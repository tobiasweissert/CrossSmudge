-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local a=dofile('main.lua');on_init();on_draw();on_update();assert(a.state().day==-1);on_exit()
