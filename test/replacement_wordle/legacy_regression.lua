-- Existing app accepts five arbitrary letters as a scored guess. CrossPlay
-- rejects words absent from its accepted list and preserves the typed row.
local saved='APPLE;;ZZZZZ;27'
smudge.load=function(key) return saved end
smudge.save=function(key,value) saved=value end
smudge.get_memory=nil
dofile('main.lua')
on_init()
on_button('confirm',true)
on_exit()
assert(saved=='APPLE;;ZZZZZ;27','invalid ZZZZZ consumed a row instead of remaining typed: '..saved)
