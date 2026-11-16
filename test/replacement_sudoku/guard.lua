-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
local exits=0
smudge.read_file=function() error('memory guard must leave old files alone') end
smudge.write_file=function() error('memory guard must not write saves') end
smudge.exit=function() exits=exits+1 end
local app=dofile('main.lua')
assert(app.unsupported, '75KiB device must use measured safe gate')
if on_init then on_init() end
on_draw()
on_button('confirm',false);assert(exits==0)
on_button('back',true);assert(exits==1)
on_touch(100,100,'tap');assert(exits==2)
if on_exit then on_exit() end
print('75KiB safe unsupported screen, exit, and no save access passed')
