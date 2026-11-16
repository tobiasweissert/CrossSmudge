local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local C=dofile('cp_core.lua');local J=dofile('cp_journal.lua')
local function boot(g)
 saved={};assert(J.open('solitaire',160,function(s)return C.restore(s)~=nil end):write(C.save(g)))
 local app=dofile('main.lua');on_init();on_draw();if app.layout().menu then on_button('confirm',true);on_draw()end;return app
end
local app=boot(C.deal(123456,true));local l=app.layout()
on_touch(l.x+l.cw//2,l.top+l.ch//2,'tap');on_draw()
assert(#app.state().piles[1]==3,'real stock tap did not draw three')
l=app.layout();assert(l.pileColumns[2]==3 and l.pileColumns[5]==6,'native foundation order differs')
assert(l.waste and l.waste.w==l.cw+2*(l.w>l.h and 36 or 18),'native three-card waste fan missing')
on_touch(l.waste.x+l.waste.w-4,l.waste.y+l.ch//2,'tap');on_draw();capture('solitaire-stock-three-selected-waste.pgm')
on_exit()
local g=C.deal(123456,false);for p=0,12 do g.piles[p]={}end
local used={};for rank=12,0,-1 do used[(rank%2<<4)|rank]=true end
for suit=0,3 do for rank=0,12 do local c=(suit<<4)|rank;if not used[c]and #g.piles[12]<6 then used[c]=true;g.piles[12][#g.piles[12]+1]=c end end end
for rank=12,0,-1 do g.piles[12][#g.piles[12]+1]=((rank%2<<4)|rank)|128 end
local pile=6
for suit=0,3 do for rank=0,12 do local c=(suit<<4)|rank;if not used[c]then if #g.piles[0]<24 then g.piles[0][#g.piles[0]+1]=c else g.piles[pile][#g.piles[pile]+1]=c|128;pile=6+(pile-5)%6 end end end end
for p=6,11 do for i=1,#g.piles[p]-1 do g.piles[p][i]=g.piles[p][i]&127 end end
g.moves=77;assert(C.restore(C.save(g)));app=boot(g);l=app.layout()
assert(l.pileOffsets and #l.pileOffsets[12]==19,'native per-card fan offsets missing')
if l.w>l.h then assert(l.pileOffsets[12][7]==30 and l.pileOffsets[12][8]==60,'native first open card strip is not preserved')end
capture('solitaire-crowded.pgm')
local x=l.x+6*(l.cw+l.gap)+l.cw//2;local y=l.tab+l.pileOffsets[12][7]+5
on_touch(x,y,'tap');on_draw();capture('solitaire-crowded-selected.pgm');assert(app.state().moves==77,'selecting a run moved the cards')
on_exit();print('Original variable fan, native foundation order, three-card waste and real picked-run touch passed')
