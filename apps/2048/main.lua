-- Keep the original app and its memory footprint on button-only devices.
if smudge.get_device and smudge.get_device() == "sticky" then
    smudge.dofile("sticky_main.lua")
else
    smudge.dofile("legacy_main.lua")
end
