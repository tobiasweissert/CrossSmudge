-- LuaRunner essential libraries intentionally omit the optional utf8 library.
utf8=utf8 or dofile('utf8_compat.lua')
-- Keep the existing provider UI byte-identical on every untargeted device.
return dofile(smudge.get_device and smudge.get_device()=='sticky' and 'sticky_main.lua' or 'legacy_main.lua')
