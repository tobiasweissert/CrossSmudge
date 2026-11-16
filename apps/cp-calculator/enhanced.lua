if smudge.get_device and smudge.get_device()=='sticky' then return dofile('sticky_enhanced.lua') end
return dofile('legacy_enhanced.lua')
