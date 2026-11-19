local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
local sticky=ok and device=='sticky'and smudge.get_memory().lua_max_kb>=512
local core=dofile('core.lua')
local ui=dofile(sticky and 'sticky_enhanced.lua'or smudge.get_memory().lua_max_kb>=512 and 'enhanced.lua' or 'ui.lua')
return ui.install(core)
