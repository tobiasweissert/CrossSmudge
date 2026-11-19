local M=dofile('core.lua');local g=M.new(123);M.new=function()return g end
collectgarbage('collect');dofile('ui.lua').install(M);on_init();on_button('confirm',true)
-- Hold ignores mode and does not reveal; changing the mode does not change that.
on_touch(400,750,'tap');on_long_press(30,110);assert(g.b[1]==4 and g.status==0)
on_long_press(30,110);assert(g.b[1]==0 and g.status==0)
local state=M.encode(g)
on_long_press(-1,110);on_long_press(480,110);on_long_press(30,84-1);on_long_press(30,684)
assert(M.encode(g)==state)
local write=smudge.write_file;local exits=0
smudge.write_file=function()return false end
smudge.exit=function()exits=exits+1 end
on_button('back',true);on_draw();capture('save-failed.pgm')
on_button('back',true);assert(exits==0)
smudge.write_file=write;on_button('back',true);assert(exits==1)
-- Confirmed reset has a cancel default and preserves the game when cancelled.
on_init();on_button('confirm',true);on_button('back',true)
on_button('down',true);on_button('down',true);on_button('confirm',true)
on_button('confirm',true);assert(M.encode(g)==state)
on_draw();capture('reset-cancelled.pgm')
print('hold mode independence, bounds, failed exit/retry and cancelled reset PASS')
