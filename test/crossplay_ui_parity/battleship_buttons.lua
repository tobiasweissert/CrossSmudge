-- Physical grid navigation reaches the original footer controls on Sticky.
dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
for i=1,10 do on_button('down',true);on_draw()end
on_button('right',true);on_draw();on_button('confirm',true);on_draw();on_exit()
local M=dofile('core.lua');local g=dofile('storage.lua').load(function(raw)
 local data=raw:match('^1|(.*)$');return data and M.decode(data)
end)
assert(g and g.phase==1 and g.ship==6 and M.valid(g),'physical Ready did not commit a full legal fleet')
assert(g.sa==string.rep('0',100)and g.sb==string.rep('0',100),'physical Ready fired a shot')
print('Physical navigation/Ready uses actual callback/journal path without firing')
