local core=dofile('core.lua')
collectgarbage('collect')
local ui=dofile('ui.lua')
ui.install(core)
