-- Hold-to-see requires the opt-in release edge; older hosts keep explicit hide controls.
if smudge.get_memory().lua_max_kb<512 then return dofile('compact.lua') end
local ok,release=pcall(smudge.has_feature,'touch_release')
return dofile(ok and release and 'enhanced.lua' or 'compact.lua')
