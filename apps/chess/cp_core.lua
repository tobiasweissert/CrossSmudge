-- CrossPlay ChessCore/ChessEngine Lua port. See PROVENANCE.md.
local C={}
local N={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}}
local K={{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}}
local R={{0,1},{1,0},{0,-1},{-1,0}}
local B={{1,1},{1,-1},{-1,-1},{-1,1}}
local function inside(f,r) return f>=0 and f<8 and r>=0 and r<8 end
local function colour(p,w) return p~=0 and ((p&8)==0)==w end
local function king(p,w) for s=0,63 do if p.squares[s]==(w and 6 or 14) then return s end end end
function C.attacked(p,s,w)
 local f,r=s&7,s>>3
 local function has(x,y,t) return inside(x,y) and p.squares[y*8+x]==(t|(w and 0 or 8)) end
 local pr=r+(w and -1 or 1)
 if has(f-1,pr,1) or has(f+1,pr,1) then return true end
 for i=1,8 do if has(f+N[i][1],r+N[i][2],2) or has(f+K[i][1],r+K[i][2],6) then return true end end
 for set=1,2 do local ds=set==1 and R or B
  for _,d in ipairs(ds) do local x,y=f+d[1],r+d[2]
   while inside(x,y) do local q=p.squares[y*8+x]
    if q~=0 then if colour(q,w) and ((q&7)==5 or (q&7)==(set==1 and 4 or 3)) then return true end break end
    x,y=x+d[1],y+d[2]
   end
  end
 end
 return false
end
function C.check(p) local s=king(p,p.whiteToMove); return s~=nil and C.attacked(p,s,not p.whiteToMove) end
local masks={[0]=2,[7]=1,[56]=8,[63]=4}
function C.make(p,m)
 local b=p.squares; local q=b[m.from]; local w=p.whiteToMove
 local u={captured=b[m.to],castling=p.castling,epSquare=p.epSquare,halfmoveClock=p.halfmoveClock}
 b[m.from]=0;b[m.to]=q
 if m.flags&4~=0 then local s=(m.from&56)|(m.to&7);u.captured=b[s];b[s]=0
 elseif m.flags&16~=0 then b[m.to]=m.promotion|(w and 0 or 8)
 elseif m.flags&8~=0 then local h=m.from&56;local ks=m.to&7==6;local a=h+(ks and 7 or 0);local z=h+(ks and 5 or 3);b[z]=b[a];b[a]=0 end
 if q&7==6 then p.castling=p.castling&~(w and 3 or 12) end
 p.castling=p.castling&~(masks[m.from] or 0)&~(masks[m.to] or 0)
 p.epSquare=m.flags&2~=0 and ((m.from+m.to)//2) or -1
 p.halfmoveClock=(q&7==1 or u.captured~=0) and 0 or math.min(255,p.halfmoveClock+1)
 if not w then p.fullmoveNumber=p.fullmoveNumber+1 end
 p.whiteToMove=not w;return u
end
function C.unmake(p,m,u)
 p.whiteToMove=not p.whiteToMove;local w=p.whiteToMove;local b=p.squares
 if not w then p.fullmoveNumber=p.fullmoveNumber-1 end
 b[m.from]=m.flags&16~=0 and (1|(w and 0 or 8)) or b[m.to];b[m.to]=0
 if m.flags&4~=0 then b[(m.from&56)|(m.to&7)]=u.captured else b[m.to]=u.captured end
 if m.flags&8~=0 then local h=m.from&56;local ks=m.to&7==6;local a=h+(ks and 7 or 0);local z=h+(ks and 5 or 3);b[a]=b[z];b[z]=0 end
 p.castling=u.castling;p.epSquare=u.epSquare;p.halfmoveClock=u.halfmoveClock
end
function C.legal(p)
 local out={};local w=p.whiteToMove;local b=p.squares
 local function add(f,t,flags,prom)
  local m={from=f,to=t,flags=flags or 0,promotion=prom or 0};local u=C.make(p,m);local s=king(p,w)
  local ok=not s or not C.attacked(p,s,not w);C.unmake(p,m,u)
  if ok then out[#out+1]=m end
 end
 local function pawn(f,t,flags,prom) if prom then for _,v in ipairs({5,4,3,2}) do add(f,t,flags|16,v) end else add(f,t,flags) end end
 for from=0,63 do local q=b[from]
  if colour(q,w) then local f,r=from&7,from>>3;local typ=q&7
   if typ==1 then local nr=r+(w and 1 or -1);local promote=nr==(w and 7 or 0)
    if inside(f,nr) and b[nr*8+f]==0 then pawn(from,nr*8+f,0,promote)
     if r==(w and 1 or 6) and b[(r+(w and 2 or -2))*8+f]==0 then add(from,(r+(w and 2 or -2))*8+f,2) end
    end
    for cf=f-1,f+1,2 do if inside(cf,nr) then local t=nr*8+cf
     if colour(b[t],not w) then pawn(from,t,1,promote) elseif b[t]==0 and t==p.epSquare then add(from,t,5) end
    end end
   elseif typ==2 or typ==6 then for _,d in ipairs(typ==2 and N or K) do local x,y=f+d[1],r+d[2]
    if inside(x,y) and not colour(b[y*8+x],w) then add(from,y*8+x,b[y*8+x]==0 and 0 or 1) end
   end
   else for set=1,2 do if not (set==1 and typ==3) and not(set==2 and typ==4) then
    for _,d in ipairs(set==1 and R or B) do local x,y=f+d[1],r+d[2]
     while inside(x,y) do local t=y*8+x;if colour(b[t],w) then break end
      local capture=b[t]~=0;add(from,t,capture and 1 or 0);if capture then break end;x,y=x+d[1],y+d[2]
     end
    end
   end end end
  end
 end
 local h=w and 0 or 56;local s=h+4
 if b[s]==(w and 6 or 14) and not C.attacked(p,s,not w) then
  if p.castling&(w and 1 or 4)~=0 and b[h+5]==0 and b[h+6]==0 and not C.attacked(p,h+5,not w) and not C.attacked(p,h+6,not w) then add(s,h+6,8) end
  if p.castling&(w and 2 or 8)~=0 and b[h+3]==0 and b[h+2]==0 and b[h+1]==0 and not C.attacked(p,h+3,not w) and not C.attacked(p,h+2,not w) then add(s,h+2,8) end
 end
 return out
end
function C.parseFen(s)
 if type(s)~='string' or #s>100 then return nil end
 local board,side,rights,ep,h,f=s:match('^(%S+) ([wb]) (%S+) (%S+)%s*(%d*)%s*(%d*)$')
 if not board then return nil end
 local p={squares={},whiteToMove=side=='w',castling=0,epSquare=-1,halfmoveClock=math.min(255,tonumber(h) or 0),fullmoveNumber=tonumber(f) or 1}
 if p.fullmoveNumber<1 or p.fullmoveNumber>65535 then return nil end
 for i=0,63 do p.squares[i]=0 end
 local r,c=7,0
 for ch in board:gmatch('.') do if ch=='/' then if c~=8 or r==0 then return nil end;r=r-1;c=0
  elseif ch:match('[1-8]') then c=c+tonumber(ch)
  else local t=('pnbrqk'):find(ch:lower(),1,true);if not t or c>7 then return nil end;p.squares[r*8+c]=t|(ch==ch:lower() and 8 or 0);c=c+1 end
  if c>8 then return nil end
 end
 if r~=0 or c~=8 then return nil end
 if rights~='-' then for ch in rights:gmatch('.') do local i=('KQkq'):find(ch,1,true);if not i then return nil end;p.castling=p.castling|(1<<(i-1)) end end
 if ep~='-' then if not ep:match('^[a-h][1-8]$') then return nil end;p.epSquare=(ep:byte(2)-49)*8+ep:byte()-97 end
 return p
end
function C.fen(p)
 local rows={}
 for r=7,0,-1 do local row='';local n=0
  for f=0,7 do local q=p.squares[r*8+f];if q==0 then n=n+1 else if n>0 then row=row..n;n=0 end;local t=('pnbrqk'):sub(q&7,q&7);row=row..(q&8==0 and t:upper() or t) end end
  if n>0 then row=row..n end;rows[#rows+1]=row
 end
 local rights='';for i=1,4 do if p.castling&(1<<(i-1))~=0 then rights=rights..('KQkq'):sub(i,i) end end
 local ep=p.epSquare<0 and '-' or string.char(97+(p.epSquare&7),49+(p.epSquare>>3))
 return table.concat(rows,'/')..(p.whiteToMove and ' w ' or ' b ')..(rights=='' and '-' or rights)..' '..ep..' '..p.halfmoveClock..' '..p.fullmoveNumber
end
function C.start() return C.parseFen('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1') end
function C.moveString(m) return string.char(97+(m.from&7),49+(m.from>>3),97+(m.to&7),49+(m.to>>3))..(m.promotion~=0 and ('pnbrqk'):sub(m.promotion,m.promotion) or '') end
function C.perft(p,d) if d<=0 then return 1 end;local n=0;for _,m in ipairs(C.legal(p)) do local u=C.make(p,m);n=n+C.perft(p,d-1);C.unmake(p,m,u) end;return n end
local values={100,320,330,500,900,20000}
local pst={
{
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10,-20,-20, 10, 10,  5,
   5, -5,-10,  0,  0,-10, -5,  5,
   0,  0,  0, 20, 20,  0,  0,  0,
   5,  5, 10, 25, 25, 10,  5,  5,
  10, 10, 20, 30, 30, 20, 10, 10,
  50, 50, 50, 50, 50, 50, 50, 50,
   0,  0,  0,  0,  0,  0,  0,  0,
},
{
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50,
},
{
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20,
},
{
   0,  0,  0,  5,  5,  0,  0,  0,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   5, 10, 10, 10, 10, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0,
},
{
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -10,  5,  5,  5,  5,  5,  0,-10,
   0,  0,  5,  5,  5,  5,  0, -5,
  -5,  0,  5,  5,  5,  5,  0, -5,
 -10,  0,  5,  5,  5,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20,
},
{
  20, 30, 10,  0,  0, 10, 30, 20,
  20, 20,  0,  0,  0,  0, 20, 20,
 -10,-20,-20,-20,-20,-20,-20,-10,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
},
}
function C.evaluate(p)
 local score=0
 for s=0,63 do local q=p.squares[s];if q~=0 then local t=q&7;local w=q&8==0;local idx=w and s or ((7-(s>>3))*8+(s&7));local v=values[t]+pst[t][idx+1];score=score+(w and v or -v) end end
 return p.whiteToMove and score or -score
end
local function moveScore(p,m)
 local v=0
 if m.flags&1~=0 then local victim=m.flags&4~=0 and 1 or (p.squares[m.to]&7);v=10000+(values[victim] or 0)*10-values[p.squares[m.from]&7] end
 if m.flags&16~=0 then v=v+9000+values[m.promotion] end
 return v
end
local function pick(p,list,i)
 local best=i;local score=moveScore(p,list[i])
 for j=i+1,#list do local s=moveScore(p,list[j]);if s>score then score=s;best=j end end
 list[i],list[best]=list[best],list[i]
end
function C.search(p,depth,budget,tick)
 depth=math.max(1,math.min(4,depth));budget=budget or 0
 local ctx={nodes=0,budgetExhausted=false,hasMove=false,score=0}
 local function negamax(d,a,b,ply)
  if budget~=0 and ctx.nodes>=budget then ctx.budgetExhausted=true;return C.evaluate(p) end
  ctx.nodes=ctx.nodes+1;if tick and ctx.nodes%32==0 then tick() end
  if d<=0 then return C.evaluate(p) end
  local list=C.legal(p);if #list==0 then return C.check(p) and -30000+ply or 0 end
  local best=-60000
  for i=1,#list do pick(p,list,i);local m=list[i];local u=C.make(p,m);local s=-negamax(d-1,-b,-a,ply+1);C.unmake(p,m,u)
   best=math.max(best,s);a=math.max(a,best);if a>=b or ctx.budgetExhausted then break end
  end
  return best
 end
 local list=C.legal(p);local a=-60000
 for i=1,#list do pick(p,list,i);local m=list[i];local u=C.make(p,m);local s=-negamax(depth-1,-60000,-a,1);C.unmake(p,m,u)
  if not ctx.hasMove or s>ctx.score then ctx.best=m;ctx.score=s;ctx.hasMove=true;a=math.max(a,s) end
  if ctx.budgetExhausted then break end
 end
 return ctx
end
-- Saved positions must be legal game states, beyond the source's permissive FEN parser.
function C.valid(p)
 if type(p)~='table' or type(p.squares)~='table' or type(p.whiteToMove)~='boolean' then return false end
 for _,v in ipairs({'castling','epSquare','halfmoveClock','fullmoveNumber'}) do if math.type(p[v])~='integer' then return false end end
 if p.castling<0 or p.castling>15 or p.epSquare< -1 or p.epSquare>63 or p.halfmoveClock<0 or p.halfmoveClock>255 or p.fullmoveNumber<1 or p.fullmoveNumber>65535 then return false end
 local wk,bk,wn,bn=nil,nil,0,0
 for i=0,63 do local q=p.squares[i]
  if math.type(q)~='integer' or q<0 or q>14 or q==7 or q==8 then return false end
  if q==6 then if wk then return false end;wk=i elseif q==14 then if bk then return false end;bk=i end
  if q~=0 then if q&8==0 then wn=wn+1 else bn=bn+1 end end
  if q&7==1 and (i<8 or i>=56) then return false end
 end
 if not wk or not bk or wn>16 or bn>16 or (math.abs((wk&7)-(bk&7))<=1 and math.abs((wk>>3)-(bk>>3))<=1) then return false end
 if C.attacked(p,p.whiteToMove and bk or wk,p.whiteToMove) then return false end
 for i=0,3 do if p.castling&(1<<i)~=0 then local white=i<2;local h=white and 0 or 56;local r=i%2==0 and 7 or 0
  if p.squares[h+4]~=(white and 6 or 14) or p.squares[h+r]~=(white and 4 or 12) then return false end
 end end
 if p.epSquare>=0 then local ep=p.epSquare;local w=p.whiteToMove
  if ep>>3~=(w and 5 or 2) or p.squares[ep]~=0 or p.squares[ep+(w and -8 or 8)]~=(w and 9 or 1) or p.squares[ep+(w and 8 or -8)]~=0 then return false end
 end
 return true
end
-- Explicit search frames: stock Lua has no coroutine library. The board displayed
-- by the activity is never the working board, including when a search is canceled.
function C.newSearch(p,depth,budget)
 local work=C.parseFen(C.fen(p));local root={root=true,depth=math.max(1,math.min(4,depth)),alpha=-60000,beta=60000,ply=0,index=1,best=-60000,list=C.legal(work),entered=true}
 return {position=work,frames={root},nodes=0,budget=budget or 60000,budgetExhausted=false,hasMove=false,score=0,done=false}
end
function C.stepSearch(job,limit)
 local p=job.position;local frames=job.frames
 local function finish(score)
  local f=table.remove(frames)
  if f.root then job.done=true;return end
  C.unmake(p,f.move,f.undo)
  local parent=frames[#frames];local s=-score
  if parent.root and (not job.hasMove or s>job.score) then job.best=f.move;job.score=s;job.hasMove=true end
  parent.best=math.max(parent.best,s);parent.alpha=math.max(parent.alpha,parent.best)
 end
 for operation=1,limit or 24 do
  if job.done then break end
  local f=frames[#frames]
  if not f.entered then
   f.entered=true
   if job.budget~=0 and job.nodes>=job.budget then job.budgetExhausted=true;finish(C.evaluate(p))
   else job.nodes=job.nodes+1
    if f.depth<=0 then finish(C.evaluate(p)) else f.list=C.legal(p)
     if #f.list==0 then finish(C.check(p) and -30000+f.ply or 0) end
    end
   end
  elseif f.index>#f.list or f.alpha>=f.beta or job.budgetExhausted then finish(f.best)
  else
   pick(p,f.list,f.index);local m=f.list[f.index];f.index=f.index+1
   local u=C.make(p,m);frames[#frames+1]={depth=f.depth-1,alpha=-f.beta,beta=-f.alpha,ply=f.ply+1,index=1,best=-60000,move=m,undo=u}
  end
 end
 return job.done
end
return C
