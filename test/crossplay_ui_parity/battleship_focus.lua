local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end;if path:match('%.dat$')then return nil end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local focused=false;local rect=smudge.rect;local l=app.layout()
smudge.rect=function(...)
 local x,y,w,h,fill,thickness,color=...
 if x==l.view.x+4 and y==l.view.y+4 and w==l.view.w-8 and h==l.view.h-8 and not fill and thickness==2 and color==false then focused=true end
 return rect(...)
end
for i=1,10 do on_button('down',true)end;on_button('right',true);on_draw()
assert(focused,'physical Ready focus is invisible')
focused=false;local at=l.placement.board:find('1',1,true)
on_touch(l.x+(at-1)%10*l.cell+l.cell//2,l.y+(at-1)//10*l.cell+l.cell//2,'tap');on_draw()
assert(not focused,'touch kept stale physical footer focus')
assert(app.state().phase==0,'focus navigation committed fleet')
on_exit()
