return dofile(smudge.get_memory().lua_max_kb>=512 and 'enhanced.lua' or 'compact.lua')
