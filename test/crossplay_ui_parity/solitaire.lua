-- Run actual Sticky callbacks, retaining the original card interaction path.
local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local band=false;local rect=smudge.rect
smudge.rect=function(x,y,w,h,filled,black,...)
 if x==0 and y==0 and w==select(1,smudge.get_bounds()) and h==56 and filled and black then band=true end
 return rect(x,y,w,h,filled,black,...)
end
local app=dofile('main.lua');on_init();on_draw();if app.layout().menu then on_button('confirm',true);on_draw()end
assert(band,'Original Solitaire has a solid 56px black header')
local l=app.layout()
if l.w>l.h then
 assert(l.cw==92 and l.ch==122 and l.x==16 and l.gap==20 and l.top==75 and l.tab==221,
  'Original landscape Solitaire card geometry differs')
end
local count=#app.state().piles[0]
on_touch(l.x+l.cw//2,l.top+l.ch//2,'tap');on_draw()
assert(#app.state().piles[0]<count,'drawn stock tap did not deal')
local undo=app.layout().buttons[3]
on_touch(undo.x+undo.w//2,undo.y+undo.h//2,'tap');on_draw()
assert(#app.state().piles[0]==count,'original header Undo is not operable')
on_exit()
print('Original Solitaire black header/card geometry/actual stock touch passed')
