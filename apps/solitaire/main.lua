-- Only Sticky uses CrossPlay; absent device API preserves the original game.
if not smudge.get_device or smudge.get_device()~='sticky'then return dofile('legacy_main.lua')end
-- Keep the already-qualified compact controller below the PSRAM UI gate.
return dofile(smudge.get_memory().lua_max_kb>=512 and 'cp_app.lua'or'compact_app.lua')
