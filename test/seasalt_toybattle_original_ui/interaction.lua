assert(smudge.get_device()=='sticky');local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw();local M=dofile('core.lua');local seen={}
local function view()return APP=='seasalt'and app.view or app.view()end
local function tap(op,value)
 for _,c in ipairs(view().controls)do if c.op==op and(value==nil or value==c.value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return true end end;error('missing '..op..' '..tostring(value))
end
local export=dofile(APP=='seasalt'and'export_seasalt.lua'or'export_toy.lua')
local function frame(name,model)on_draw();capture(name..'.pgm');if model then export(app,name)end;assert(smudge.write_file(name..'.txt',M.encode(app.state()),false))end
if APP=='seasalt'then
 tap('new');frame('ss-initial',true)
 local function move(m)
  local g=app.state();local moves=M.moves(g,nil,true);local index
  for i,v in ipairs(moves)do if v[1]==m[1]and v[2]==m[2]and v[3]==m[3]then index=i;break end end
  if m[1]==5 then
   tap('select',m[2]);frame('ss-selected-one',true);tap('select',m[3]);frame('ss-selected-pair',true);tap('duo')
  elseif m[1]==9 then tap('call');frame('ss-call-'..m[2],false);assert(smudge.write_file('ss-call-'..m[2]..'.points',tostring(M.points(g,1)),false));tap('act',assert(index))
  else tap('act',assert(index))end
 end
 for turn=1,1500 do
  local g=app.state();assert(M.valid(g));on_draw()
  if g.phase==3 then frame('ss-match',false);dofile('export_round.lua')(app,'ss-match');break end
  if g.phase==2 then frame('ss-round-'..g.round,false);dofile('export_round.lua')(app,'ss-round-'..g.round);tap('act',1)
  elseif g.turn==2 then on_update()
  else
   local key='ss-step-'..g.step;if not seen[key]then frame(key,true);seen[key]=true end
   if g.step==3 and app.view.tiles[1]then
    local c=app.view.tiles[1].c;tap('select',c);local selected=M.encode(g);frame('ss-one-rule',true);assert(M.encode(g)==selected);tap('select',c)
   end
   local intent=M.ai(g);if intent[1]==9 and not seen.lastChance then intent={9,1};seen.lastChance=true end;move(intent)
  end
 end
 assert(app.state().phase==3,'SeaSalt match did not finish')
else
 tap('home',1);tap('start');frame('tb-initial',true)
 tap('brief');frame('tb-brief',false);on_button('back',true);on_draw()
 for turn=1,1000 do
  local g=app.state();assert(M.valid(g));on_draw();local v=app.view();local d=v.draft;local D=dofile('ui_draft.lua');local ask=D.pending(M,g,d)
  if g.phase==1 then frame('tb-ended-board',true);tap('result');frame('tb-result',true);break end
  if g.turn==2 then frame('tb-thinking',true);on_update()
  elseif ask=='troop'then
   local L=app.layout();local selected=false
   for i,k in ipairs(L.kinds)do
    local shown=D.project(M,g,d);local terrain=M.board(g);local legal=false
    for p=1,terrain.n+#terrain.hq do if M.canplace(shown,1,p,k,true)then legal=true;break end end
    if legal then local b=L.rack[i];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();if D.pending(M,g,d)~='troop'then selected=true;break end end
   end
   if not selected then assert(#d.steps==0,'Captain chain stranded');tap('draw')end
  elseif ask=='slot'or ask=='jumbo'or ask=='recall'or ask=='shoveFrom'or ask=='shoveTo'then
   if not seen[ask]then frame('tb-'..ask,true);seen[ask]=true end
   local candidates=D.candidates(M,g,d);local p=next(candidates);assert(p,'targeted question without reachable answer '..ask);local pos=app.layout().points[p];on_touch(pos[1],pos[2],'tap')
  elseif ask=='exhume'then
   if not seen[ask]then frame('tb-'..ask,true);seen[ask]=true end
   local L=app.layout();local k=L.kinds[1];assert(k);local b=L.rack[1];on_touch(b.x+b.w//2,b.y+b.h//2,'tap')
  else
   if not seen[ask]then frame('tb-'..ask,true);seen[ask]=true end
   tap('take')
  end
 end
 assert(app.state().phase==1,'ToyBattle game did not finish')
end
local saved=M.encode(app.state());on_exit();assert(smudge.get_orientation()==before);local again=dofile('main.lua');on_init();assert(M.encode(again.state())==saved,'save/reopen changed state');on_exit();assert(smudge.get_orientation()==before)
