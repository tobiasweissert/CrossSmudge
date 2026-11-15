smudge={get_device=function()return 'x3' end,has_feature=function()return false end,request_update=function()end}
dofile=function()error('unsupported device loaded game')end
assert(loadfile('apps/cp-connections/main.lua'))();on_init()
print('PASS C3 entry gate: no game modules, network, or user files opened')
