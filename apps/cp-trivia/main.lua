-- Keep the existing provider UI byte-identical on every untargeted device.
return dofile(smudge.get_device and smudge.get_device()=='sticky' and 'sticky_main.lua' or 'legacy_main.lua')
