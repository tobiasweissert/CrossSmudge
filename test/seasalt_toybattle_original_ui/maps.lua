assert(APP=='toybattle'and smudge.get_device()=='sticky');local M,S=dofile('core.lua'),dofile('storage.lua');local D=dofile('ui_draft.lua');local export=dofile('export_toy.lua');local seen={};local app
local function tap(op,value)
 for _,c in ipairs(app.view().controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing control '..op)
end
local function frame(name)
 on_draw();capture(name..'.pgm');export(app,name);assert(smudge.write_file(name..'.txt',M.encode(app.state()),false))
end
for map=0,9 do
 local S=dofile('storage.lua');S.load(M.decode);local saved=M.new(41+map*7919,map);assert(M.valid(saved));assert(S.save(M.encode(saved)))
 app=nil;app=dofile('main.lua');collectgarbage('collect');on_init();on_draw();tap('home',1);assert(app.state().map==map);frame('map-'..map..'-initial')
 tap('brief');on_draw();capture('map-'..map..'-brief.pgm');on_button('back',true)
 local states={}
 for turn=1,1400 do
  local g=app.state();assert(M.valid(g));on_draw();local d=app.view().draft;local ask=D.pending(M,g,d)
  if g.phase==1 then frame('map-'..map..'-ended');tap('result');frame('map-'..map..'-result');break end
  if g.turn==2 then on_update()
  else
   if not states[ask]then frame('map-'..map..'-'..ask);states[ask]=true;seen[ask]=(seen[ask]or 0)+1 end
   if ask=='troop'then
    local L=app.layout();local selected=false
    for i,k in ipairs(L.kinds)do local shown=D.project(M,g,d);local t=M.board(g);local legal=false;for p=1,t.n+#t.hq do if M.canplace(shown,1,p,k,true)then legal=true;break end end
     if legal then local b=L.rack[i];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();if D.pending(M,g,d)~='troop'then selected=true;break end end
    end
    if not selected then assert(#d.steps==0,'stranded chain map '..map);tap('draw')end
   elseif ask=='slot'or ask=='jumbo'or ask=='recall'or ask=='shoveFrom'or ask=='shoveTo'then
    local p=next(D.candidates(M,g,d));assert(p,'no choice for '..ask..' map '..map);local point=app.layout().points[p];on_touch(point[1],point[2],'tap')
   elseif ask=='exhume'then
    local L=app.layout();local accepted=false
    for i,k in ipairs(L.kinds)do if D.discardLeft(M,g,d,k)>0 then local b=L.rack[i];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');if D.pending(M,g,d)~='exhume'then accepted=true;break end end end
    assert(accepted,'no raised choice')
   else tap('take')end
  end
 end
 assert(app.state().phase==1,'map failed to end '..map);local raw=M.encode(app.state());on_exit();assert(M.decode(raw));app=nil;collectgarbage('collect')
end
local out={};for ask,n in pairs(seen)do out[#out+1]=ask..' '..n end;assert(smudge.write_file('questions.txt',table.concat(out,'\n'),false))
