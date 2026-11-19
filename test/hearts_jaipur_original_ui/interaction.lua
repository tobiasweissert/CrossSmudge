assert(smudge.get_device()=='sticky')
local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw()
local V,M=app.view,dofile('core.lua')
local function tap(op,value)
 for _,c in ipairs(V.controls)do if c.op==op and(value==nil or value==c.value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing drawn control '..op..' '..tostring(value))
end
local function frame(name)on_draw();capture(name..'.pgm');assert(smudge.write_file(name..'.txt',M.encode(app.state()),false))end
if APP=='hearts'then
 tap('resume');local g=app.state();local old=g.board;tap('act',1);assert(#g.selection==1 and g.board==old);frame('selected-one')
 tap('act',2);tap('act',3);assert(#g.selection==3 and g.board==old);frame('selected-three')
 tap('act',14);assert(g.phase==1 and #g.selection==0);frame('after-pass')
 local snapshots={};local tick=123456;smudge.millis=function()return tick end
 for step=1,15000 do
  g=app.state();assert(M.valid(g));on_draw()
  if g.phase==4 then frame('game-over');break elseif g.phase==3 then if not snapshots.score then frame('hand-over');snapshots.score=true end;tap('act',1)end
  if g.phase==0 then frame('passing-'..g.handno%4);tap('act',1);tap('act',2);tap('act',3);tap('act',#M.hand(g,1)+1)end
  if g.phase==1 and g.turn==1 then
   local illegal;for _,c in ipairs(V.controls)do if c.op=='refused'then illegal=c.value;break end end
   if illegal and not snapshots.refused then local raw=M.encode(g);tap('refused',illegal);assert(M.encode(g)==raw);snapshots.refused=true end
   local n=0;for _,c in ipairs(V.controls)do if c.op=='act'then n=c.value;break end end;assert(n>0);tap('act',n)
  else tick=tick+901;on_update()end
  g=app.state()
  if g.phase==1 and not snapshots.playing then frame('playing');snapshots.playing=true end
  if g.phase==2 and not snapshots.trick then frame('trick-taken');snapshots.trick=true end
 end
 assert(app.state().phase==4,'game did not finish')
else
 tap('new');local g=app.state();local raw=M.encode(g);local camel
 for i,k in ipairs(g.market)do if k==7 then camel=i;break end end
 assert(camel);tap('market',camel);assert(V.camel and V.canCommit);frame('camels-selected');tap('market',camel);assert(not V.camel and M.encode(g)==raw)
 -- Two-card exchange via actual controls, including camels as payment.
 local selected={};for i,k in ipairs(g.market)do if k<=6 then selected[#selected+1]=i end end;assert(#selected>=2)
 for _,i in ipairs(selected)do tap('market',i)end
 local wanted={};for _,i in ipairs(selected)do wanted[g.market[i]]=true end
 local need=#selected;for k=1,7 do local have=k==7 and g.herd[1]or g.hands[1][k];if not wanted[k]then for n=1,math.min(have,need)do tap('hand',k);need=need-1 end end end
 assert(need==0 and V.canCommit,'multi-card exchange unreachable');frame('trade-selected');tap('commit');assert(M.encode(g)~=raw);on_update();on_draw();frame('after-trade')
 for round=1,3 do
 local guard=0
 while app.state().phase==0 do
  guard=guard+1;assert(guard<400,'round failed to end');g=app.state();assert(M.valid(g));on_draw()
  if g.turn==2 then on_update()else
   local move=M.ai(g);assert(move)
   if move[1]==1 then tap('market',move[2])elseif move[1]==2 then for i,k in ipairs(g.market)do if k==7 then tap('market',i);break end end
   else for i=1,move[3]do tap('hand',move[2])end end
   assert(V.canCommit);tap('commit')
  end
 end
 frame('round-'..round..'-ended-board');tap('commit');frame('round-'..round..'-scores');assert(V.showScores);if app.state().phase==2 then break end;tap('act',1);assert(app.state().phase==0);frame('next-round')
 end
 assert(app.state().phase==2,'match did not finish')
end
local saved=M.encode(app.state());local phase=app.state().phase;on_exit();assert(smudge.get_orientation()==before)
local reopened=dofile('main.lua');on_init();on_draw();assert(reopened.state().phase==phase and M.encode(reopened.state())==saved,'save/reopen altered the game');assert(reopened.view.record.played==1,'completed match counted twice or lost');capture('reopened-menu.pgm');on_exit();assert(smudge.get_orientation()==before)
