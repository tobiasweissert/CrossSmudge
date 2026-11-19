if smudge.get_device and smudge.get_device()=='sticky' then return dofile('original_graphics.lua') end
return dofile('legacy_graphics.lua')
