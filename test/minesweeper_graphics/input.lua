assert(smudge.get_device()=='sticky')
local M=dofile('core.lua');local g=M.new(1);M.new=function()return g end
collectgarbage('collect');dofile('ui.lua').install(M);on_init();on_button('confirm',true)
test_core=M;test_game=g
collectgarbage('collect')
dofile('input_cases.lua')
