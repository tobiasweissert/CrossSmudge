local core=dofile('core.lua')
local ui=dofile(smudge.get_memory().lua_max_kb>=512 and 'enhanced.lua' or 'ui.lua')
return ui.install(core)
