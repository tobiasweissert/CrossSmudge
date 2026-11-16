-- CrossPlay SolitaireCore translated to Lua. MIT; see PROVENANCE.md.
local C={}
local function rank(c) return c&15 end
local function suit(c) return (c>>4)&3 end
local function up(c) return c&128~=0 end
local function red(c) return suit(c)%2==1 end
local function tab(i) return i>=6 and i<=12 end
local function foundation(i) return i>=2 and i<=5 end
C.rankOf=rank;C.suitOf=suit;C.isFaceUp=up;C.isRed=red
function C.shuffledDeck(seed)
 local a={};for s=0,3 do for r=0,12 do a[#a+1]=(s<<4)|r end end
 local rng=seed==0 and -1640531527 or seed
 for i=52,2,-1 do rng=rng~(rng<<13);rng=rng~(rng>>17);rng=rng~(rng<<5)
  local j=((rng&0x7fffffff)%i+(rng<0 and ((0x7fffffff%i+1)%i) or 0))%i+1;a[i],a[j]=a[j],a[i]
 end
 return a
end
function C.deal(seed,three)
 local g={piles={},moves=0,seed=seed,drawThree=three,history={}}
 for i=0,12 do g.piles[i]={} end
 local deck=C.shuffledDeck(seed);local n=1
 for col=0,6 do for row=0,col do local p=g.piles[6+col];p[#p+1]=deck[n]|(row==col and 128 or 0);n=n+1 end end
 for i=n,52 do g.piles[0][#g.piles[0]+1]=deck[i] end
 return g
end
function C.won(g) for i=2,5 do if #g.piles[i]~=13 then return false end end;return true end
function C.runLength(g,p,i)
 local a=g.piles[p];if not a or i<0 or i>=#a or not up(a[i+1]) then return 0 end
 if not tab(p) then return i==#a-1 and 1 or 0 end
 for j=i+1,#a-1 do if not up(a[j+1]) or red(a[j])==red(a[j+1]) or rank(a[j+1])~=rank(a[j])-1 then return 0 end end
 return #a-i
end
function C.canMove(g,from,to,count)
 local a,b=g.piles[from],g.piles[to]
 if not a or not b or from==to or count<=0 or count%1~=0 or to<2 or from==0 or count>#a or #b+count>24 then return false end
 if C.runLength(g,from,#a-count)~=count then return false end
 local c=a[#a-count+1];local top=b[#b]
 if foundation(to) then return count==1 and ((not top and rank(c)==0) or (top and suit(top)==suit(c) and rank(c)==rank(top)+1)) or false end
 return (not top and rank(c)==12) or (top and up(top) and red(c)~=red(top) and rank(c)==rank(top)-1) or false
end
local function push(g,m)
 if #g.history==96 then table.remove(g.history,1) end
 g.history[#g.history+1]=(m.from or 0)|((m.to or 0)<<4)|(m.count<<8)|(m.flipped and 8192 or 0)|(m.stock and 16384 or 0);g.moves=g.moves+1
end
function C.move(g,from,to,count)
 if not C.canMove(g,from,to,count) then return false end
 local a,b=g.piles[from],g.piles[to];local first=#a-count+1
 for i=first,#a do b[#b+1]=a[i] end;for i=#a,first,-1 do a[i]=nil end
 local flipped=tab(from) and #a>0 and not up(a[#a]);if flipped then a[#a]=a[#a]|128 end
 push(g,{from=from,to=to,count=count,flipped=flipped});return true
end
function C.drawFromStock(g)
 local a,b=g.piles[0],g.piles[1]
 if #a==0 then if #b==0 then return false end
  while #b>0 do a[#a+1]=table.remove(b)&127 end;push(g,{stock=true,count=0})
 else local n=math.min(#a,g.drawThree and 3 or 1);for i=1,n do b[#b+1]=table.remove(a)|128 end;push(g,{stock=true,count=n}) end
 return true
end
function C.undo(g)
 local packed=table.remove(g.history);if not packed then return false end
 local m={from=packed&15,to=(packed>>4)&15,count=(packed>>8)&31,flipped=packed&8192~=0,stock=packed&16384~=0};g.moves=g.moves-1
 if m.stock then local a,b=g.piles[0],g.piles[1]
  if m.count==0 then while #a>0 do b[#b+1]=table.remove(a)|128 end else for i=1,m.count do a[#a+1]=table.remove(b)&127 end end
 else local a,b=g.piles[m.from],g.piles[m.to];if m.flipped then a[#a]=a[#a]&127 end
  local first=#b-m.count+1;for i=first,#b do a[#a+1]=b[i] end;for i=#b,first,-1 do b[i]=nil end
 end
 return true
end
function C.foundationFor(g,from) if not g.piles[from] then return -1 end;for i=2,5 do if i~=from and C.canMove(g,from,i,1) then return i end end;return -1 end
function C.canAutoFinish(g)
 if C.won(g) or #g.piles[0]>0 then return false end
 for i=6,12 do for _,c in ipairs(g.piles[i]) do if not up(c) then return false end end end;return true
end
function C.autoPlay(g)
 local moved=0;local progress=true
 while progress do progress=false
  for i=1,12 do if not foundation(i) then local dest=C.foundationFor(g,i);if dest>=0 and C.move(g,i,dest,1) then moved=moved+1;progress=true end end end
 end
 return moved
end
function C.save(g)
 local out={tostring(g.seed),tostring(g.moves),g.drawThree and '1' or '0'}
 for i=0,12 do local a={};for _,c in ipairs(g.piles[i]) do a[#a+1]=string.char(c) end;local s=table.concat(a);out[#out+1]=string.char(#s)..s end
 return table.concat(out,'|',1,3)..'|'..table.concat(out,'',4)
end
function C.restore(s)
 if type(s)~='string' or #s>160 then return nil end
 local seed,moves,three,at=s:match('^(%-?%d+)|(%d+)|([01])|()');seed=tonumber(seed);moves=tonumber(moves)
 if not seed or not moves or moves>1000000 or math.type(seed)~='integer' then return nil end
 local g={seed=seed,moves=moves,drawThree=three=='1',piles={},history={}};local seen,total={},0
 for i=0,12 do local n=s:byte(at);if not n or n>24 then return nil end;at=at+1;local a={};g.piles[i]=a
  local faceSeen=false
  for j=1,n do local c=s:byte(at);at=at+1
   if not c or c&64~=0 or rank(c)>12 or seen[c&127] then return nil end;seen[c&127]=true;total=total+1;a[j]=c
   if i==0 and up(c) or i>=1 and i<=5 and not up(c) then return nil end
   if foundation(i) and (rank(c)~=j-1 or (j>1 and suit(c)~=suit(a[1]))) then return nil end
   if tab(i) then if faceSeen and not up(c) then return nil end;faceSeen=faceSeen or up(c)
    if j>1 and up(a[j-1]) and (red(c)==red(a[j-1]) or rank(c)~=rank(a[j-1])-1) then return nil end
   end
  end
  if tab(i) and n>0 and not up(a[n]) then return nil end
 end
 if total~=52 or at~=#s+1 then return nil end
 return g
end
return C
