-- UI-only move questions. Every completed/probed answer is accepted by the unchanged engine.
local copy=dofile('util.lua').copy;local D={}
local function sum(t)local n=0;for _,v in ipairs(t)do n=n+v end;return n end
function D.clear()return{steps={},step=1,effect=false,base=false,question='troop'}end
function D.project(M,g,d)
 local shown=copy(g)
 for i,st in ipairs(d.steps)do if st[1]==0 then break end;local one=copy(st);one[5]=0
  if one[2]==3 or i==d.step and not d.effect and one[2]~=5 then one[3]=0;one[4]=0 end
  if not M.apply(shown,{one})then break end;shown.turn=g.turn
 end
 return shown
end
function D.legal(M,g,d,steps)
 local probe=copy(g);if M.apply(probe,steps)then return true end
 -- A Captain offer must leave a real next placement available before it is shown.
 local last=steps[#steps];if last and last[2]==3 and last[3]==1 and #steps<4 then
  local shown=D.project(M,g,{steps=steps,step=#steps,effect=true});local t=M.board(shown)
  for k=1,8 do for p=1,t.n+#t.hq do if M.canplace(shown,g.turn,p,k,true)then local next=copy(steps);next[#next+1]={p,k,M.canplace(shown,g.turn,p,k,false)and 0 or 1,0,0,0,0,1};local trial=copy(g);if M.apply(trial,next)then return true end end end end
 end;return false
end
function D.discardLeft(M,g,d,k)
 local n=g.discard[g.turn][k];local t=M.board(g)
 for i=1,d.step-1 do local st=d.steps[i];if st and st[5]==1 and t.special[st[1]]==4 and st[8]==k then n=n-1 end end;return math.max(0,n)
end
function D.pending(M,g,d)
 if g.phase~=0 then return'ready'end
 local st=d.steps[d.step];if not st then return'troop'end;if st[1]==0 then return'slot'end
 local shown=D.project(M,g,d);local t=M.board(g);local seat=g.turn;local p,k=st[1],st[2]
 if p>t.n then return'ready'end
 if not d.effect and not(g.special==1 and t.special[p]==7)then
  if k==4 then for q=1,t.n do if t.adj[p]&(1<<(q-1))~=0 and M.top(shown,q)==3-seat then return'jumbo'end end
  elseif k==2 or k==7 then if shown.taken[seat]<20 and sum(shown.rack[seat])<8 then return'draw'end
  elseif k==6 then if sum(shown.rack[3-seat])>0 then return'steal'end
  elseif k==3 and #d.steps<4 then local trial=copy(d.steps);trial[d.step][3]=1;if D.legal(M,g,d,trial)then return'chain'end end
 end
 if not d.base and g.special==1 then
  local special=t.special[p]
  if special==2 and shown.taken[seat]<20 and sum(shown.rack[seat])<8 then return'base'end
  if special==5 and sum(shown.rack[3-seat])>0 then return'base'end
  if special==4 and sum(shown.rack[seat])<8 then for k=1,8 do if D.discardLeft(M,g,d,k)>0 then return'exhume'end end end
  if special==1 and sum(shown.rack[seat])<8 then for q=1,t.n do if q~=p and M.top(shown,q)==seat then return'recall'end end end
  if special==3 then if st[5]==1 and st[6]>0 then return'shoveTo'end;for q=1,t.n do if t.adj[p]&(1<<(q-1))~=0 and M.top(shown,q)==3-seat then return'shoveFrom'end end end
 end
 if k==3 and st[3]==1 then d.step=d.step+1;d.effect=false;d.base=false;return'troop'end
 return'ready'
end
function D.candidates(M,g,d)
 local ask=D.pending(M,g,d);local shown=D.project(M,g,d);local t=M.board(g);local st=d.steps[d.step];local out={}
 for p=1,t.n+#t.hq do local legal=false
  if ask=='slot'then legal=M.canplace(shown,g.turn,p,st[2],true)
  elseif ask=='jumbo'then legal=p<=t.n and t.adj[st[1]]&(1<<(p-1))~=0 and M.top(shown,p)==3-g.turn
  elseif ask=='recall'then legal=p<=t.n and p~=st[1]and M.top(shown,p)==g.turn
  elseif ask=='shoveFrom'then legal=p<=t.n and t.adj[st[1]]&(1<<(p-1))~=0 and M.top(shown,p)==3-g.turn
  elseif ask=='shoveTo'then legal=p<=t.n and t.adj[st[6]]&(1<<(p-1))~=0 end
  if legal then local trial=copy(d.steps);local s=trial[d.step]
   if ask=='slot'then s[1]=p;s[3]=M.canplace(shown,g.turn,p,s[2],false)and 0 or 1
   elseif ask=='jumbo'then s[3]=1;s[4]=p
   elseif ask=='recall'then s[5]=1;s[6]=p
   elseif ask=='shoveFrom'then
    for q=1,t.n do if t.adj[p]&(1<<(q-1))~=0 then local probe=copy(trial);probe[d.step][5]=1;probe[d.step][6]=p;probe[d.step][7]=q;if D.legal(M,g,d,probe)then out[p]=true;break end end end
   else s[5]=1;s[7]=p end
   if ask~='shoveFrom'and D.legal(M,g,d,trial)then out[p]=true end
  end
 end;return out
end
function D.troop(M,g,d,k)
 local ask=D.pending(M,g,d);local shown=D.project(M,g,d)
 if ask=='exhume'then if D.discardLeft(M,g,d,k)==0 then return false end;local trial=copy(d.steps);trial[d.step][5]=1;trial[d.step][8]=k;if not D.legal(M,g,d,trial)then return false end;d.steps=trial;d.base=true;return true end
 if ask~='troop'or shown.rack[g.turn][k]==0 or shown.frozen[g.turn]==k then return false end
 local old=d.steps[d.step];d.steps[d.step]={0,k,0,0,0,0,0,1};local candidates=D.candidates(M,g,d);if not next(candidates)then d.steps[d.step]=old;return false end;return true
end
function D.slot(M,g,d,p)
 if not D.candidates(M,g,d)[p]then return false end;local ask=D.pending(M,g,d);local st=d.steps[d.step]
 if ask=='slot'then local shown=D.project(M,g,d);st[1]=p;st[3]=M.canplace(shown,g.turn,p,st[2],false)and 0 or 1
 elseif ask=='jumbo'then st[3]=1;st[4]=p;d.effect=true
 elseif ask=='recall'then st[5]=1;st[6]=p;d.base=true
 elseif ask=='shoveFrom'then st[5]=1;st[6]=p
 elseif ask=='shoveTo'then st[7]=p;d.base=true end;return true
end
function D.offer(M,g,d,take)
 local ask=D.pending(M,g,d);local st=d.steps[d.step];if not st then return false end
 if ask=='draw'or ask=='steal'or ask=='chain'then local trial=copy(d.steps);trial[d.step][3]=take and 1 or 0;if not D.legal(M,g,d,trial)then return false end;d.steps=trial;d.effect=true
 elseif ask=='jumbo'then if take then return false end;st[3]=0;d.effect=true
 elseif ask=='base'then local trial=copy(d.steps);trial[d.step][5]=take and 1 or 0;if not D.legal(M,g,d,trial)then return false end;d.steps=trial;d.base=true
 elseif ask=='recall'or ask=='shoveFrom'or ask=='shoveTo'or ask=='exhume'then if take then return false end;st[5]=0;st[6]=0;st[7]=0;d.base=true
 else return false end;return true
end
function D.prompt(M,g,d)
 local ask=D.pending(M,g,d);local st=d.steps[d.step]
 return({troop=d.step>1 and'AND ONE MORE'or'TAP A TROOP, OR DRAW TWO',slot='TAP A BASE',jumbo='REMOVE AN ADJACENT TROOP?',draw='REINFORCEMENTS?',steal='SHOOT INTO THEIR RACK?',chain='PLACE A SECOND TROOP?',recall='CALL ONE OF YOURS HOME?',shoveFrom='SHOVE ONE OF THEIRS?',shoveTo='SHOVE IT WHERE?',exhume='RAISE ONE FROM THE DISCARD?',base='USE THE BASE?',ready=''})[ask]
end
function D.whyTroop(M,g,d,k)
 if D.pending(M,g,d)~='troop'then return''end;local shown=D.project(M,g,d)
 if shown.rack[g.turn][k]==0 then return'YOU ARE NOT HOLDING THAT'end
 if shown.frozen[g.turn]==k then return'PINNED: IT SITS OUT THIS TURN'end
 return'NO PATH TO THERE FROM YOUR H.Q.'
end
function D.whySlot(M,g,d,p)
 local ask=D.pending(M,g,d);if ask~='slot'then return ask=='ready'and''or'NOT ONE OF THE CHOICES'end
 local t=M.board(g);local shown=D.project(M,g,d);local k=d.steps[d.step][2]
 if p>t.n and t.hq[p-t.n]==g.turn then return'THAT IS YOUR OWN H.Q.'end
 if g.special==1 and t.gate[p]~=0 and t.gate[p]&(1<<(k-1))==0 then return'THIS BASE TAKES OTHER VALUES'end
 local holder,def=M.top(shown,p);if holder~=0 and holder~=g.turn and not(k==1 or def==1 or k>def)then return'TOO WEAK TO COVER THAT ONE'end
 if g.special==1 and t.special[p]==7 and k==5 then return'EFFECTS DO NOT WORK ON THAT BASE'end
 return'NO PATH TO THERE FROM YOUR H.Q.'
end
return D
