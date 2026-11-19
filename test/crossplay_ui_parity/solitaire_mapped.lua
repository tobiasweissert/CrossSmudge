local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local C=dofile('cp_core.lua');local J=dofile('cp_journal.lua')
local function boot(g)
 saved={};if g then assert(J.open('solitaire',160,function(s)return C.restore(s)~=nil end):write(C.save(g)))end
 local app=dofile('main.lua');on_init();on_draw();return app
end
local function shot(app,name,selected,card)
 local g=app.state();local r=app.records();local recent={};for i=1,16 do recent[i]=tostring(r.recent[i]or 0)end
 local fields={g.drawThree and 1 or 0,g.moves,selected or -1,card or -1,r.played,r.wins,r.streak,table.concat(recent)}
 for p=0,12 do fields[#fields+1]=#g.piles[p];for _,c in ipairs(g.piles[p])do fields[#fields+1]=c end end
 for i,v in ipairs(fields)do fields[i]=tostring(v)end
 assert(write('solitaire-'..name..'.state',table.concat(fields,' '),false));capture('solitaire-'..name..'.pgm')
end
local app=boot();shot(app,'menu',0);on_button('confirm',true);on_draw();shot(app,'board');on_exit()
app=boot(C.deal(123456,true));on_button('confirm',true);on_draw();local l=app.layout();on_touch(l.x+3,l.top+3,'tap');on_draw();shot(app,'stock-three')
l=app.layout();on_touch(l.waste.x+l.waste.w-4,l.waste.y+3,'tap');on_draw();shot(app,'stock-three-selected',1,2);on_exit()
local g=C.deal(123456,false);for p=0,12 do g.piles[p]={}end
local used={};for rank=12,0,-1 do used[(rank%2<<4)|rank]=true end
for suit=0,3 do for rank=0,12 do local c=(suit<<4)|rank;if not used[c]and #g.piles[12]<6 then used[c]=true;g.piles[12][#g.piles[12]+1]=c end end end
for rank=12,0,-1 do g.piles[12][#g.piles[12]+1]=((rank%2<<4)|rank)|128 end
local pile=6;for suit=0,3 do for rank=0,12 do local c=(suit<<4)|rank;if not used[c]then if #g.piles[0]<24 then g.piles[0][#g.piles[0]+1]=c else g.piles[pile][#g.piles[pile]+1]=c|128;pile=6+(pile-5)%6 end end end end
for p=6,11 do for i=1,#g.piles[p]-1 do g.piles[p][i]=g.piles[p][i]&127 end end
g.moves=77;assert(C.restore(C.save(g)));app=boot(g);on_button('confirm',true);on_draw();shot(app,'crowded')
l=app.layout();on_touch(l.x+6*(l.cw+l.gap)+3,l.tab+l.pileOffsets[12][7]+3,'tap');on_draw();shot(app,'crowded-selected',12,6);on_exit()
for p=0,12 do g.piles[p]={}end;for suit=0,3 do for rank=0,12 do g.piles[2+suit][#g.piles[2+suit]+1]=(suit<<4)|rank|128 end end
g.moves=73;app=boot(g);assert(C.won(app.state()));assert(app.layout().won,'original completed-deal screen missing');assert(app.records().played==1 and app.records().wins==1,'observed durable win was not counted');shot(app,'won');on_exit()
