local orientation='landscape';smudge.get_orientation=function()return orientation end;smudge.set_orientation=function(o)orientation=o end;smudge.get_bounds=function()return orientation=='portrait'and 480 or 800,orientation=='portrait'and 800 or 480 end
local app=dofile('main.lua');on_init();assert(orientation=='portrait');local M,V,g=dofile('core.lua'),app.view,app.state()
local function draw()
 local before=M.encode(g);on_draw();assert(M.encode(g)==before and M.valid(g));for _,c in ipairs(V.controls)do local b=c.box;assert(b.x>=0 and b.y>=0 and b.x+b.w<=480 and b.y+b.h<=800,'touch control outside screen')end
end
local function tap(op,value)
 for _,c in ipairs(V.controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return end end;error('missing target '..op)
end
draw();tap('help');for i=1,8 do V.help=i;draw()end;tap('menu');draw();tap('new');g=app.state();draw();assert(not V.menu)
-- Selection commits through the exact market and status targets.
local before=M.encode(g);local i;for n,k in ipairs(g.market)do if k<7 and M.legal(g,{1,n})then i=n;break end end;assert(i);tap('market',i);draw();assert(M.encode(g)==before and V.canCommit);tap('commit');assert(g.turn==2);draw()
V.menu=true;before=M.encode(g);on_update();assert(before==M.encode(g),'AI behind menu');V.menu=false;on_update();assert(g.turn==1)
-- Private hand/bonus mutations do not alter the board draw calls/text.
local bitmap,rect,round,line,circle=smudge.bitmap,smudge.rect,smudge.rounded_rect,smudge.line,smudge.circle
local function digest()
 local data={};local function rec(...)local t={...};for n,v in ipairs(t)do t[n]=tostring(v)end;data[#data+1]=table.concat(t,':')end
 smudge.bitmap=rec;smudge.rect=rec;smudge.rounded_rect=rec;smudge.line=rec;smudge.circle=rec;on_draw();smudge.bitmap=bitmap;smudge.rect=rect;smudge.rounded_rect=round;smudge.line=line;smudge.circle=circle;return table.concat(data,'\n')
end
local a=digest();g.bpoints[2]=g.bpoints[2]+9;local first,second
for k=1,6 do if g.hands[2][k]>0 then first=k;second=k%6+1;break end end
assert(first);g.hands[2][first]=g.hands[2][first]-1;g.hands[2][second]=g.hands[2][second]+1;assert(a==digest(),'hidden goods or bonus leaked');g.hands[2][first]=g.hands[2][first]+1;g.hands[2][second]=g.hands[2][second]-1;g.bpoints[2]=g.bpoints[2]-9
V.inspect=true;draw();V.inspect=false
-- Exercise a real exchange selected from hand/herd and market geometry.
local swap=false;local phases={};local turns=0
while g.phase~=2 and turns<1200 do
 draw();phases[g.phase]=true;turns=turns+1
 if g.phase==1 then tap('act',1)
 elseif g.turn==2 then on_update()
 else
  local chosen
  if not swap then
   for mask=1,31 do local taken={};local n=0;for j=1,5 do if mask&(1<<(j-1))~=0 then taken[g.market[j]]=true;n=n+1 end end
    if n>=2 and not taken[7]and not taken[8]then local give={0,0,0,0,0,0,0};local left=n;for k=1,7 do if not taken[k]then local count=k==7 and g.herd[1]or g.hands[1][k];give[k]=math.min(count,left);left=left-give[k]end end;local m={3,mask,give};if M.legal(g,m)then chosen=m;break end end
   end
  end
  if chosen then for j=1,5 do if chosen[2]&(1<<(j-1))~=0 then tap('market',j);draw()end end
   for k=1,7 do for j=1,chosen[3][k]do tap('hand',k);draw()end end;assert(V.canCommit);tap('commit');swap=true
  else assert(M.apply(g,M.ai(g)));V.clear()end
 end
end
assert(g.phase==2 and phases[1]and swap);draw();before=M.encode(g);on_exit();assert(orientation=='landscape');on_update();on_touch(100,100,'tap');assert(M.encode(g)==before);assert(dofile('storage.lua').load(M.decode));print('Jaipur all phases/rules, market take/exchange, private goods/bonuses, AI pause, orientation restore, checked save')
