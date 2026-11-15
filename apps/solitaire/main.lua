-- Only Sticky uses CrossPlay; absent device API preserves the original game.
return dofile(smudge.get_device and smudge.get_device() == "sticky" and "cp_app.lua" or "legacy_main.lua")
