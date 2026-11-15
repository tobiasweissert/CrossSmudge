local C=dofile('cp_core.lua')
local seed=0x61753129
local function rand(n) seed=seed~(seed<<13);seed=seed~(seed>>17);seed=seed~(seed<<5);return (seed&0x7fffffff)%n+1 end
local plies,terminal=0,0
for game=1,8 do
 local p=C.start()
 for ply=1,400 do
  local before=C.fen(p)
  local list=C.legal(p)
  assert(C.fen(p)==before,'legal mutated position')
  if #list==0 or p.halfmoveClock>=100 then terminal=terminal+1;break end
  local m=list[rand(#list)]
  assert((p.squares[m.to]&7)~=6,'king capture')
  local u=C.make(p,m);local after=C.fen(p)
  local moverWhite=not p.whiteToMove;local king
  for sq=0,63 do if p.squares[sq]==(moverWhite and 6 or 14) then king=sq end end
  assert(king and not C.attacked(p,king,p.whiteToMove),'moved into check')
  C.unmake(p,m,u);assert(C.fen(p)==before,'undo mismatch')
  C.make(p,m);assert(C.fen(p)==after,'replay mismatch')
  p=assert(C.parseFen(after));plies=plies+1
  list=nil;collectgarbage('collect')
 end
end
-- Complete Fool's Mate, including checkmate precedence over draw counters.
local p=C.start()
for _,uci in ipairs({'f2f3','e7e5','g2g4','d8h4'}) do
 local found
 for _,m in ipairs(C.legal(p)) do if C.moveString(m)==uci then found=m;break end end
 assert(found);C.make(p,found)
end
assert(C.check(p) and #C.legal(p)==0)
local r=C.search(p,4,60000);assert(not r.hasMove and r.nodes==0)
print('random legal plies '..plies..'; naturally ended games '..terminal..'; full checkmate game passed')
