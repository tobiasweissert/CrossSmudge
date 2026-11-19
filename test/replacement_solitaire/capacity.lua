-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
local a=dofile('main.lua');on_init();on_draw();if a.layout().menu then on_button('confirm',true);on_draw()end;collectgarbage('step',50)
for i=1,120 do on_button('confirm',true);collectgarbage('step',50);collectgarbage('step',0);on_update();on_draw();collectgarbage('step',50) end
assert(a.state().moves==120)
on_exit()
