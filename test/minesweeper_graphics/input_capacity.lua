assert(smudge.get_device()=='sticky')
local M=dofile('core.lua');local g=M.new(1);M.new=function()return g end
collectgarbage('collect');dofile('ui.lua').install(M);on_init();on_button('confirm',true)
input_frame('down',30,110,0);input_frame('tick',0,0,499);assert(g.b[1]==0)
input_frame('tick',0,0,500);assert(g.b[1]==4)
input_frame('tick',0,0,900);input_frame('release',0,0,1000)
assert(g.b[1]==4 and g.status==0)
print('75KiB production input: hold once, flag only, release no dig PASS')
