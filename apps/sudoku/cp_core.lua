-- Direct CrossPlay SudokuCore technique ladder and symmetric dig. MIT; PROVENANCE.md.
local C={names={'ONLY DIGIT','ONLY CELL','LOCKED','CELL PAIR','DIGIT PAIR','CELL TRIO','DIGIT TRIO','RECTANGLE','Y CHAIN','SWORDFISH'}}
local peers,units,rows,cols,boxes={},{},{},{},{}
for i=1,81 do rows[i]=(i-1)//9;cols[i]=(i-1)%9;boxes[i]=(rows[i]//3)*3+cols[i]//3 end
local function peer(a,b) return a~=b and (rows[a]==rows[b] or cols[a]==cols[b] or boxes[a]==boxes[b]) end
for i=1,81 do peers[i]={};for j=1,81 do if peer(i,j) then peers[i][#peers[i]+1]=j end end end
for u=0,26 do units[u]={};for s=0,8 do local r,c
 if u<9 then r,c=u,s elseif u<18 then r,c=s,u-9 else local b=u-18;r,c=(b//3)*3+s//3,(b%3)*3+s%3 end
 units[u][s+1]=r*9+c+1
end end
local counts,low={},{}
for m=0,511 do local n=0;for d=1,9 do if m&(1<<(d-1))~=0 then n=n+1;if not low[m] then low[m]=d end end end;counts[m]=n end
local function bit(d) return 1<<(d-1) end
local function assign(g,i,d)
 if i<1 or i>81 or d<1 or d>9 then return false end
 if g.v[i]==0 and g.c[i]&bit(d)==0 then return false end
 g.v[i]=d;g.c[i]=0;local ok=true
 for _,p in ipairs(peers[i]) do if g.v[p]==d then ok=false elseif g.v[p]==0 then g.c[p]=g.c[p]&~bit(d);if g.c[p]==0 then ok=false end end end
 return ok
end
local function load(a)
 local g={v={},c={},broken=false};for i=1,81 do g.v[i]=0;g.c[i]=511 end
 for i=1,81 do if type(a[i])~='number' or a[i]%1~=0 or a[i]<0 or a[i]>9 then g.broken=true
 elseif a[i]~=0 and not assign(g,i,a[i]) then g.broken=true end end
 return g
end
local function eliminate(g,i,b)
 if g.v[i]~=0 then return false end;local a=g.c[i]&~b
 if a==g.c[i] then return false end;g.c[i]=a;if a==0 then g.broken=true end;return true
end
local function positions(g,u,d)
 local m=0;for s,i in ipairs(units[u]) do if g.v[i]==0 and g.c[i]&bit(d)~=0 then m=m|(1<<(s-1)) end end;return m
end
local function placed(g,u,d) for _,i in ipairs(units[u]) do if g.v[i]==d then return true end end;return false end
local function complete(g) for i=1,81 do if g.v[i]==0 then return false end end;return true end
local function naked(g) for i=1,81 do if g.v[i]==0 and counts[g.c[i]]==1 then return i,low[g.c[i]] end end end
local function hidden(g)
 for u=0,26 do for d=1,9 do if not placed(g,u,d) then local m=positions(g,u,d)
  if counts[m]==1 then local i=units[u][low[m]];if counts[g.c[i]]~=1 then return i,d,u end end
 end end end
end
local function combinations(n,k,fn)
 local idx={};for i=1,k do idx[i]=i end
 while true do if fn(idx) then return true end
  local i=k;while i>=1 and idx[i]==n-k+i do i=i-1 end;if i<1 then return false end
  idx[i]=idx[i]+1;for j=i+1,k do idx[j]=idx[j-1]+1 end
 end
end
local function locked(g)
 for b=0,8 do for d=1,9 do local u=b+18
  if not placed(g,u,d) then local m=positions(g,u,d)
   if counts[m]>=2 then local r,c;local sr,sc=true,true
    for s,i in ipairs(units[u]) do if m&(1<<(s-1))~=0 then if r==nil then r=rows[i] elseif r~=rows[i] then sr=false end;if c==nil then c=cols[i] elseif c~=cols[i] then sc=false end end end
    local progress=false
    if sr then for _,i in ipairs(units[r]) do if boxes[i]~=b then progress=eliminate(g,i,bit(d)) or progress end end end
    if sc then for _,i in ipairs(units[c+9]) do if boxes[i]~=b then progress=eliminate(g,i,bit(d)) or progress end end end
    if progress then return true end
   end
  end
 end end
 for u=0,17 do for d=1,9 do if not placed(g,u,d) then local m=positions(g,u,d)
  if counts[m]>=2 then local b;local same=true
   for s,i in ipairs(units[u]) do if m&(1<<(s-1))~=0 then if not b then b=boxes[i] elseif b~=boxes[i] then same=false end end end
   if same and b then local progress=false
    for _,i in ipairs(units[b+18]) do if not ((u<9 and rows[i]==u) or (u>=9 and cols[i]==u-9)) then progress=eliminate(g,i,bit(d)) or progress end end
    if progress then return true end
   end
  end
 end end end
 return false
end
local function subset(g,k,isHidden)
 for u=0,26 do local items,where={},{}
  if isHidden then for d=1,9 do if not placed(g,u,d) then local m=positions(g,u,d);if counts[m]>=2 and counts[m]<=k then items[#items+1]=d;where[#items]=m end end end
  else for _,i in ipairs(units[u]) do if g.v[i]==0 then items[#items+1]=i end end end
  if #items>=(isHidden and k or k+1) then
   if combinations(#items,k,function(idx)
    local span,wanted=0,0
    for _,j in ipairs(idx) do if isHidden then span=span|where[j];wanted=wanted|bit(items[j])
     else local m=g.c[items[j]];if counts[m]<2 or counts[m]>k then return false end;span=span|m end
    end
    if counts[span]~=k then return false end
    local progress=false
    if isHidden then for s,i in ipairs(units[u]) do if span&(1<<(s-1))~=0 then progress=eliminate(g,i,511&~wanted) or progress end end
    else for j,i in ipairs(items) do local chosen=false;for _,v in ipairs(idx) do if j==v then chosen=true end end;if not chosen then progress=eliminate(g,i,span) or progress end end end
    return progress
   end) then return true end
  end
 end
 return false
end
local function fish(g,k)
 for orientation=0,1 do for d=1,9 do local lines,cross={},{}
  for i=0,8 do local u=i+orientation*9;if not placed(g,u,d) then local m=positions(g,u,d);if counts[m]>=2 and counts[m]<=k then lines[#lines+1]=i;cross[#lines]=m end end end
  if #lines>=k and combinations(#lines,k,function(idx)
   local covered=0;for _,j in ipairs(idx) do covered=covered|cross[j] end;if counts[covered]~=k then return false end
   local progress=false
   for s=0,8 do if covered&(1<<s)~=0 then for i=0,8 do local chosen=false;for _,j in ipairs(idx) do if lines[j]==i then chosen=true end end
    if not chosen then local cell=orientation==0 and (i*9+s+1) or (s*9+i+1);progress=eliminate(g,cell,bit(d)) or progress end
   end end end
   return progress
  end) then return true end
 end end
 return false
end
local function xywing(g)
 for pivot=1,81 do if g.v[pivot]==0 and counts[g.c[pivot]]==2 then local a=low[g.c[pivot]];local b=low[g.c[pivot]&~bit(a)]
  for _,first in ipairs(peers[pivot]) do if g.v[first]==0 and counts[g.c[first]]==2 then local ha=g.c[first]&bit(a)~=0;local hb=g.c[first]&bit(b)~=0
   if ha~=hb then local shared=ha and a or b;local other=ha and b or a;local third=low[g.c[first]&~bit(shared)]
    if third and third~=other then local wanted=bit(other)|bit(third)
     for _,second in ipairs(peers[pivot]) do if second~=first and g.v[second]==0 and g.c[second]==wanted then local progress=false
      for i=1,81 do if i~=pivot and i~=first and i~=second and peer(i,first) and peer(i,second) then progress=eliminate(g,i,bit(third)) or progress end end
      if progress then return true end
     end end
    end
   end
  end end
 end end
 return false
end
local function apply(g,r)
 if r==1 then local progress=false;while true do local i,d=naked(g);if not i then return progress end;progress=true;if not assign(g,i,d) then g.broken=true;return true end end
 elseif r==2 then local i,d=hidden(g);if not i then return false end;if not assign(g,i,d) then g.broken=true end;return true
 elseif r==3 then return locked(g)
 elseif r==4 then return subset(g,2,false) elseif r==5 then return subset(g,2,true)
 elseif r==6 then return subset(g,3,false) elseif r==7 then return subset(g,3,true)
 elseif r==8 then return fish(g,2) elseif r==9 then return xywing(g) elseif r==10 then return fish(g,3) end
 return false
end
function C.solve(a,ceiling,tick)
 local g=load(a);local report={grid=g.v,solved=false,broken=g.broken,hardest=0,steps=0}
 if g.broken then return report end
 while report.steps<4096 do
  if complete(g) then report.solved=true;return report end
  local progress=false
  for r=1,math.min(10,ceiling or 10) do if tick then tick() end;if apply(g,r) then report.steps=report.steps+1;report.hardest=math.max(report.hardest,r);progress=true;break end end
  if g.broken then report.broken=true;return report end;if not progress then return report end
 end
 return report
end
function C.next_hint(a,ceiling)
 local g=load(a);if g.broken then return {found=false} end;local hardest=0
 for guard=1,4096 do
  local i,d=naked(g);local u=-1;local via=1
  if not i then i,d,u=hidden(g);via=2 end
  if i then return {found=true,cell=i,digit=d,technique=math.max(via,hardest),unit=u or -1} end
  local progress=false
  for r=3,math.min(10,ceiling or 10) do if apply(g,r) then hardest=math.max(r,hardest);progress=true;break end end
  if not progress or g.broken then break end
 end
 return {found=false}
end
function C.levelOf(r) return r<=2 and 0 or r==3 and 1 or r<=5 and 2 or 3 end
-- Unsigned modulo without float32 rounding of the xorshift high bit.
local function mod32(x,n) return (x&0x7fffffff)%n+(x<0 and ((0x7fffffff%n+1)%n) or 0) end
function C.newGenerator(level,seed)
 local j={level=level,rng=seed,phase='fill',cell=1,guard=0,order={},nexts={},v={}}
 local function rand(n)
  if j.rng==0 then j.rng=-1640531527 end
  local r=j.rng;r=r~(r<<13);r=r~(r>>17);r=r~(r<<5);j.rng=r;return mod32(r,n)%n
 end
 j.rand=rand;j.seed=seed
 for i=1,81 do local a={1,2,3,4,5,6,7,8,9};for k=9,2,-1 do local z=rand(k)+1;a[k],a[z]=a[z],a[k] end;j.order[i]=a;j.nexts[i]=1;j.v[i]=0 end
 return j
end
function C.stepGenerator(j)
 if j.done then return true end
 if j.phase=='fill' then
  for probe=1,128 do
   if j.cell>81 then
    j.given={};for i=1,81 do j.given[i]=j.v[i] end
    j.leaders={};for i=1,41 do j.leaders[i]=i end
    for i=41,2,-1 do local k=j.rand(i)+1;j.leaders[i],j.leaders[k]=j.leaders[k],j.leaders[i] end
    j.order=nil;j.nexts=nil;j.phase='carve';j.at=1;return false
   end
   j.guard=j.guard+1;if j.guard>200000 then j.done=true;return true end
   local i=j.cell;local found=false
   while j.nexts[i]<=9 do local d=j.order[i][j.nexts[i]];j.nexts[i]=j.nexts[i]+1;local fits=true
    for _,p in ipairs(peers[i]) do if j.v[p]==d then fits=false;break end end
    if fits then j.v[i]=d;found=true;break end
   end
   if found then j.cell=i+1 else j.nexts[i]=1;j.v[i]=0;if i==1 then j.done=true;return true end;j.cell=i-1;j.v[i-1]=0 end
  end
 elseif j.phase=='carve' then
  if j.at<=41 then local i=j.leaders[j.at];local k=82-i;local a,b=j.given[i],j.given[k];j.given[i],j.given[k]=0,0
   local r=C.solve(j.given,10);if not r.solved or r.broken then j.given[i],j.given[k]=a,b end;j.at=j.at+1
  else j.report=C.solve(j.given,10);j.report.level=C.levelOf(j.report.hardest);j.report.rng=j.rng;j.report.seed=j.seed
   j.accepted=j.report.solved and not j.report.broken and j.report.level==j.level;j.done=true
  end
 end
 return j.done or false
end
function C.generate(level,seed,attempts,tick)
 local report={rng=seed}
 for attempt=1,attempts or 1 do local j=C.newGenerator(level,report.rng)
  while not C.stepGenerator(j) do if tick then tick() end end
  report=j.report or {rng=j.rng};if j.accepted then return j.given,j.v,report end
 end
 return nil,nil,report
end
function C.candidates(a,i) return load(a).c[i] end
return C
