local native=smudge.dofile
smudge.dofile=function(p) return native('apps/2048/'..p) end
smudge.get_device=function() return 'sticky' end
local state={}
smudge.save=function(k,v) state[k]=v end
smudge.load=function(k,d) return state[k] or d end
smudge.log=function() end
os.time=function() return 2048 end
native('apps/2048/main.lua');on_init()
for i=1,100 do on_swipe(({'up','down','left','right'})[i%4+1]);on_draw() end
on_exit()
print('PASS: one app, 100 production draws and moves')
