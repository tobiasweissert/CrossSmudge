assert(APP=='toybattle'and smudge.get_device()=='sticky');local M=dofile('core.lua');local U=dofile('util.lua');local D=dofile('ui_draft.lua');local export=dofile('export_toy.lua');local app
local function tap(op)
 for _,c in ipairs(app.view().controls)do if c.op==op then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing '..op)
end
local function frame(name)on_draw();capture(name..'.pgm');export(app,name);assert(smudge.write_file(name..'.txt',M.encode(app.state()),false))end
local function prepare(kind)
 if kind=='human-win'then
  for seed=1,100 do local g=M.new(seed,0);for n=1,250 do if g.phase==1 then break end;assert(M.apply(g,M.ai(g)))end;if g.phase==1 and g.winner==1 then return M.new(seed,0)end end
 else
  local g=M.new(938,0)
  for n=1,200 do
   local move={};local s=g.turn
   if g.taken[s]==20 or U.sum(g.rack[s])==8 then
    local own;for p=1,M.board(g).n do if M.top(g,p)==s then own=p;break end end
    for _,m in ipairs(M.moves(g))do if #m>0 and(not own or m[1][1]==own)then move=m;break end end;assert(#move>0)
   end
   local trial=U.copy(g);assert(M.apply(trial,move));assert(M.valid(trial));if trial.phase==1 then assert(trial.ending==3,'stack-only fixture ended unexpectedly');return g,move end;g=trial
  end
 end;error('end fixture not found '..kind)
end
for _,kind in ipairs({'human-win','stuck'})do
 local g,intent=prepare(kind);local S=dofile('storage.lua');S.load(M.decode);assert(S.save(M.encode(g)));app=dofile('main.lua');on_init();on_draw();tap('home')
 for guard=1,600 do
  g=app.state();assert(M.valid(g));on_draw();if g.phase==1 then frame(kind..'-board');tap('result');frame(kind..'-result');break end
  if g.turn==2 then on_update()
  else
   local move=intent or M.ai(g);local d=app.view().draft;local ask=D.pending(M,g,d)
   if #move==0 then tap('draw')
   elseif ask=='troop'then local k=move[d.step][2];local at;for i,v in ipairs(app.layout().kinds)do if v==k then at=app.layout().rack[i];break end end;assert(at);on_touch(at.x+at.w//2,at.y+at.h//2,'tap')
   elseif ask=='slot'then local p=app.layout().points[move[d.step][1]];on_touch(p[1],p[2],'tap')
   elseif(ask=='draw'or ask=='steal'or ask=='chain')and move[d.step][3]==1 then tap('take')else tap('skip')end
  end
 end
 assert(app.state().phase==1,'not ended '..kind);assert(kind~='human-win'or app.state().winner==1,'not human winner '..app.state().winner);assert(kind~='stuck'or app.state().ending==3,'not stuck '..app.state().ending);on_exit();app=nil;collectgarbage('collect')
end
