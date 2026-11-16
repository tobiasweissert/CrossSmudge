-- Only Sticky uses CrossPlay; absent device API preserves the original game.
local sticky=smudge.get_device and smudge.get_device()=="sticky"
return dofile(sticky and (smudge.get_memory().lua_max_kb>=512 and "enhanced.lua" or "cp_app.lua") or "legacy_main.lua")
