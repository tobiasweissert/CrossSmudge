-- Actual imported-day gameplay and touch keyboard; no model setters.
local read,write=smudge.read_file,smudge.write_file;local saved={}
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
local function shot(name)
 on_draw();capture('wordle-'..name..'.pgm');assert(write('wordle-'..name..'.state',app.core.save(app.state().game)..'\n',false))
end
local function tap(id)local b=app.layout().keys[id];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local ids={Q=1,A=11,P=10,L=19,E=3,Y=6,C=23,R=4,N=26}
local function typeWord(word)for letter in word:gmatch('.')do tap(ids[letter])end end
shot('blank');tap(1);shot('typed');tap(20)
typeWord('ALLEY');tap(28);shot('alley');typeWord('APPLE');tap(28);shot('win')
on_exit()
