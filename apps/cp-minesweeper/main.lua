local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
local sticky=ok and device=='sticky'and smudge.get_memory().lua_max_kb>=512
local core=dofile('core.lua')
collectgarbage('collect')
local ui=dofile(sticky and 'sticky_ui.lua'or'ui.lua')
return ui.install(core)
