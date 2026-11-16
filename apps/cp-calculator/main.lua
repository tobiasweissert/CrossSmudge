-- Keep the constrained frontend on C3 hosts; original masks load only with PSRAM.
local ok,m=pcall(smudge.get_memory)
if ok and m and m.lua_max_kb>=512 then return dofile('enhanced.lua') end
return dofile('compact.lua')
