-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
dofile('main.lua');on_init();on_draw();on_update();on_exit()
