assert(APP=='toybattle'and smudge.get_device()=='sticky');local M=dofile('core.lua');local U=dofile('util.lua');local D=dofile('ui_draft.lua');local export=dofile('export_toy.lua');local app
local function tap(op)
 for _,c in ipairs(app.view().controls)do if c.op==op then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing control '..op)
end
local function frame(name)on_draw();capture(name..'.pgm');export(app,name);assert(smudge.write_file(name..'.txt',M.encode(app.state()),false))end
local function prepare(map,special)
 for seed=1,500 do
  local start=M.new(seed,map);local t=M.board(start);local target
  for p=1,t.n do if t.special[p]==special then target=p;break end end
  if target and start.rack[1][5]>0 then
   for _,first in ipairs(M.moves(start))do if #first>0 and first[1][2]~=5 then
    local g=U.copy(start)
    if M.apply(g,first)then
     local opponent
     if special==3 then
      if g.rack[2][5]>0 then for p=1,t.n do if t.adj[target]&(1<<(p-1))~=0 and M.canplace(g,2,p,5,true)then opponent={{p,5,M.canplace(g,2,p,5,false)and 0 or 1,0,0,0,0,1}};break end end end
     elseif special==4 then
      if g.rack[2][6]>0 then for _,m in ipairs(M.moves(g))do if #m>0 and m[1][2]==6 then opponent=U.copy(m);opponent[1][3]=1;break end end end
     else opponent={}end
     if opponent and M.apply(g,opponent)and g.rack[1][5]>0 and M.canplace(g,1,target,5,true)and(special~=4 or U.sum(g.discard[1])>0)and M.valid(g)then return g,target end
    end
   end end
  end
 end;error('no natural saved fixture for base '..special)
end
for _,case in ipairs({{0,1,'recall'},{2,2,'base'},{3,3,'shoveFrom'},{4,4,'exhume'},{5,5,'base'}})do
 local map,special,expected=case[1],case[2],case[3];local g,target=prepare(map,special);local S=dofile('storage.lua');S.load(M.decode);assert(S.save(M.encode(g)));app=nil;app=dofile('main.lua');collectgarbage('collect');on_init();on_draw();tap('home');local original=M.encode(app.state());local L=app.layout();local rack
 for i,k in ipairs(L.kinds)do if k==5 then rack=L.rack[i];break end end;assert(rack);on_touch(rack.x+rack.w//2,rack.y+rack.h//2,'tap');on_draw();assert(D.pending(M,app.state(),app.view().draft)=='slot');local p=app.layout().points[target];on_touch(p[1],p[2],'tap');on_draw()
 assert(D.pending(M,app.state(),app.view().draft)==expected,'base question '..special);frame('base-'..special..'-'..expected);assert(M.encode(app.state())==original,'draft mutated durable state')
 for step=1,4 do local state=app.state();local d=app.view().draft;local ask=D.pending(M,state,d);if state.turn==2 or state.phase==1 then break end
  if ask=='base'then tap('take')
  elseif ask=='exhume'then local b=app.layout().rack[1];on_touch(b.x+b.w//2,b.y+b.h//2,'tap')
  else local at=next(D.candidates(M,state,d));assert(at);local point=app.layout().points[at];on_touch(point[1],point[2],'tap')end
  on_draw();if D.pending(M,app.state(),app.view().draft)=='shoveTo'then frame('base-3-shoveTo')end
 end
 assert(app.state().turn==2 or app.state().phase==1,'base could not finish');assert(M.valid(app.state()));frame('base-'..special..'-applied');on_exit();app=nil;collectgarbage('collect')
end
