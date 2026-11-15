-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
local reads,writes,exits=0,0,0
local read,write,exit=smudge.read_file,smudge.write_file,smudge.exit
smudge.read_file=function(...) reads=reads+1;return read(...) end
smudge.write_file=function(...) writes=writes+1;return write(...) end
smudge.exit=function(...) exits=exits+1;return exit(...) end
local app=dofile('main.lua')
if app.unsupported then
 assert(smudge.get_memory().lua_max_kb<256);on_draw();on_button('back',false);assert(exits==0);on_button('back',true);assert(exits==1);on_touch(10,10,'tap');assert(exits==2)
 assert(reads==0 and writes==0,'unsupported device touched save files')
 print('75KiB unsupported-device refusal passed; chess gameplay NOT supported');return
end
on_init();on_draw();collectgarbage('collect')
local l=app.layout()
on_touch(l.x+4*l.size+l.size//2,l.y+6*l.size+l.size//2,'tap')
on_touch(l.x+4*l.size+l.size//2,l.y+4*l.size+l.size//2,'tap')
assert(app.state().position.squares[28]==1,'e2-e4 input')
on_update();on_button('back',true);assert(not app.state().job,'search canceled');on_exit();on_update()
print('lean launch/draw/move/cancel/exit passed')
