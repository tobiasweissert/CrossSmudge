local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();if app.layout().menu then on_button('confirm',true);on_draw()end;local l=app.layout()
local mark=false;local round=smudge.rounded_rect
smudge.rounded_rect=function(...)
 local x,y,w,h,r,fill,color,weight=...
 if x==l.x and y==l.tab and w==l.cw and h==l.ch and r==8 and not fill and color and weight==6 then mark=true end
 return round(...)
end
on_touch(l.x+l.cw//2,l.tab+l.ch//2,'tap');on_draw()
assert(app.state().moves==0,'source card selection moved automatically')
assert(mark,'original picked run outline is missing')
if capture then capture('solitaire-selected.pgm')end
on_exit()
