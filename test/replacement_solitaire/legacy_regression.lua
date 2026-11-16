-- Before replacement this fails: stock dispatch supplies (x,y,event), not (event,x,y).
w,h=480,800
show_reset_confirm=false
is_won=false
local dealt=0
function flip_stock() dealt=dealt+1 end
dofile('input.lua')
on_touch(40,150,'tap')
assert(dealt==1,'stock tap ignored: callback arguments must be x,y,event')
