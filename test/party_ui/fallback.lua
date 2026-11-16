local original=smudge.read_file
smudge.read_file=function(path,...)assert(not path:match('^art/'),'compact path loaded presentation masks');return original(path,...)end
smudge.has_feature=function()return false end
local app=dofile('main.lua');assert(app==nil,'old host selected hold-only presentation')
on_init();on_draw();on_exit()
print('PASS compact fallback without optional release callback')
