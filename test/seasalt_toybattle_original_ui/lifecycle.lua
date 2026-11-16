assert(smudge.get_device()=='sticky');local original=smudge.write_file;local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw();local M=dofile('core.lua');local saved=M.encode(app.state());local exits=0;smudge.exit=function()exits=exits+1 end
smudge.write_file=function(path,...)if path=='state-a.dat'or path=='state-b.dat'then return false end;return original(path,...)end
on_button('back',true);on_draw();capture('failed-save-menu.pgm');assert(exits==0 and M.encode(app.state())==saved,'failed save exited or changed game')
smudge.write_file=original;on_button('back',true);assert(exits==1,'successful retry did not exit');assert(smudge.get_orientation()==before)
local writes=0;smudge.write_file=function(...)writes=writes+1;return original(...)end;on_touch(100,100,'tap');on_button('confirm',true);on_update();assert(writes==0,'closed app handled input')
smudge.write_file=original;local again=dofile('main.lua');on_init();on_draw();capture('reopened-saved-menu.pgm');assert(M.encode(again.state())==saved,'journal reopen changed state');on_exit();assert(smudge.get_orientation()==before)
