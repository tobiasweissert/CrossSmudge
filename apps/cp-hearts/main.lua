-- Original list UI stays within the constrained-device budget.
return dofile(smudge.get_memory().lua_max_kb>=512 and 'enhanced.lua' or 'compact.lua')
