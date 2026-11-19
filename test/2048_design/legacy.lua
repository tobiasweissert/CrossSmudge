local state={board='2,2,4,8,16,32,64,128,0,0,0,0,0,0,0,0',score='12',game_over='0'}
local file = os.getenv('APP2048_SOURCE')
local device = os.getenv('APP2048_DEVICE')
if device ~= 'missing' then smudge.get_device=function() return device end else smudge.get_device=nil end
smudge.load=function(k,d) return state[k] or d end
smudge.save=function(k,v) state[k]=v;print('save',k,v) end
smudge.exit=function() print('exit') end
for _,name in ipairs({'clear','header','rect','rect_dither','text','centered_text','button_hints'}) do
 smudge[name]=function(...) print(name,...) end
end
local native=smudge.dofile
smudge.dofile=function(p) return native('apps/2048/'..p) end
os.time=function() return 2048 end
native(file);on_init()
for step=1,4 do
 math.randomseed(step);on_swipe(({'left','right','up','down'})[step]);on_draw();on_exit()
end
on_button('confirm',true);on_draw();on_button('back',true);on_exit()
on_button('confirm',true);on_button('confirm',true);on_exit()
print('PASS: legacy single-instance input/draw/save trace')
