-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local a=dofile('main.lua');on_init();on_draw()
for i=1,200 do on_button('down',true);collectgarbage('step',50);on_button('confirm',true);collectgarbage('step',50);collectgarbage('step',0);on_update();on_draw();collectgarbage('step',50) end
on_exit()
