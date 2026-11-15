assert(TEXT:find('0 of 4',1,true));on_button('confirm',true);draw();assert(TEXT:find('1 of 4',1,true));restart();assert(TEXT:find('1 of 4',1,true))
DAY=16;on_update();draw();assert(TEXT:find('0 of 4',1,true));on_button('confirm',true);draw();assert(TEXT:find('2 days',1,true))
click('+');enter('Learn Lua');draw();assert(TEXT:find('of 5',1,true));restart();on_button('down',true);draw()
local M=MODULES['core.lua'];local s={names={'a','b','c'},history={[740240]=7}};M.remove(s,2);assert(s.history[740240]==3 and #s.names==2)
assert(not M.decode('a\n999999:ff\n'));assert(not M.decode('a\n740240:fg\n'))

on_button('left',true);click('Remove selected');click('Cancel');click('Remove selected');click('Delete habit');restart();assert(TEXT:find('of 4',1,true))
