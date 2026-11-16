-- Actual imported-day gameplay and touch keyboard; no model setters.
local read=smudge.read_file;local saved={}
smudge.get_date=function()return {year=2026,month=10,day=2}end
smudge.read_file=function(path,cap,offset)
 local value=saved[path]
 if value then return value:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:sub(1,1)=='/'then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local band=false;local rect=smudge.rect
smudge.rect=function(x,y,w,h,filled,black,...)
 if x==0 and y==0 and w==select(1,smudge.get_bounds()) and h==76 and filled and black then band=true end
 return rect(x,y,w,h,filled,black,...)
end
local app=dofile('main.lua');local day=app.core.dayIndex(2026,10,2)
saved['wordle.ans']=string.rep('.',day*5)..'APPLE'
saved['wordle.words']='ALLEYAPPLECRANESTONE'
on_init();on_draw()
assert(band,'Original Wordle has a76px black title band and Jersey glyphs')
local l=app.layout()
if l.w<l.h then assert(l.tile==60 and l.x==74 and l.y==121 and l.ky==537 and l.kh==74,'Original portrait Wordle geometry differs')end
local q=l.keys[1];on_touch(q.x+q.w//2,q.y+q.h//2,'tap');on_draw()
assert(app.state().game.typing=='Q','drawn keyboard typed wrong letter')
local erase=l.keys[20];on_touch(erase.x+erase.w//2,erase.y+erase.h//2,'tap');on_draw()
assert(app.state().game.typing=='','drawn erase key failed')
if capture then capture('wordle-empty.pgm')end
local ids={A=11,P=10,L=19,E=3}
for letter in ('APPLE'):gmatch('.')do local b=app.layout().keys[ids[letter]];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local enter=app.layout().keys[28];on_touch(enter.x+enter.w//2,enter.y+enter.h//2,'tap');on_draw()
assert(app.state().game.status=='Won')
local next=app.layout().result[2]
on_touch(next.x+next.w//2,next.y+next.h//2,'tap');on_draw()
assert(app.state().day==day,'disabled Next switched to unavailable puzzle')
saved['wordle.ans']=saved['wordle.ans']..'CRANE';on_draw()
on_touch(next.x+next.w//2,next.y+next.h//2,'tap');on_draw()
assert(app.state().day==day+1 and app.state().game.answer=='CRANE','original Next did not open available next day')
on_exit()
