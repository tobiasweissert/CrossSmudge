local C=dofile('cp_core.lua')
assert(math.maxinteger==2147483647 and string.packsize('n')==4,'firmware Lua ABI')
local function fields(s)
 local t={} for v in (s..'\t'):gmatch('(.-)\t') do t[#t+1]=v end return t
end
local total=tonumber(assert(smudge.read_file('oracle-count',20)))
for n=0,total-1 do
 local data=assert(smudge.read_file('oracle-'..n..'.tsv',32768))
 local p,original,moves,idx
 for line in data:gmatch('[^\n]+') do
  local f=fields(line)
  if f[1]=='CASE' then
   p=assert(C.parseFen(f[3]),f[3]);original=C.fen(p)
   assert(original==f[3], 'FEN round trip '..n)
   assert(C.evaluate(p)==tonumber(f[4]),'evaluation '..n)
   assert(C.check(p)==(f[5]=='1'),'check '..n)
   assert(C.perft(p,n<12 and 2 or 1)==tonumber(f[6]),'perft '..n)
   assert(C.fen(p)==original,'perft mutated board')
   moves=C.legal(p);idx=0
  elseif f[1]=='ATTACK' then
   for c=0,1 do for sq=0,63 do local idx=c*64+sq+1;assert(C.attacked(p,sq,c==1)==(f[2]:sub(idx,idx)=='1'),'attack '..n..' '..sq) end end
  elseif f[1]=='MOVE' then
   idx=idx+1;local m=assert(moves[idx],'missing move '..n..' '..idx)
   assert(C.moveString(m)==f[2],'move order '..n..' '..idx)
   assert(m.from==tonumber(f[3]) and m.to==tonumber(f[4]) and m.promotion==tonumber(f[5]) and m.flags==tonumber(f[6]),'move fields')
   local u=C.make(p,m);assert(C.fen(p)==f[7],'move result '..n..' '..idx)
   C.unmake(p,m,u);assert(C.fen(p)==original,'unmake '..n..' '..idx)
  elseif f[1]=='SEARCH' then
   local r=C.search(p,tonumber(f[2]),tonumber(f[3]))
   assert(r.hasMove==(f[8]=='1'),'search hasMove '..n)
   assert((r.hasMove and C.moveString(r.best) or '-')==f[4],'search move '..n..' '..f[2]..' '..f[3])
   assert(r.score==tonumber(f[5]),'search score '..n)
   assert(r.nodes==tonumber(f[6]),'search nodes '..n)
   assert(r.budgetExhausted==(f[7]=='1'),'budget '..n)
   assert(C.fen(p)==original,'search mutated board')
   local job=C.newSearch(p,tonumber(f[2]),tonumber(f[3]));local steps=0
   repeat steps=steps+1;assert(steps<20000,'async search stalled');C.stepSearch(job,(n%3)+1);assert(C.fen(p)==original,'async mutated displayed board') until job.done
   assert(job.hasMove==r.hasMove and job.score==r.score and job.nodes==r.nodes and job.budgetExhausted==r.budgetExhausted,'async source result '..n)
   assert((job.hasMove and C.moveString(job.best) or '-')==f[4],'async move '..n)
   assert(C.fen(job.position)==original,'async failed to unmake working board')
   local canceled=C.newSearch(p,4,60000);C.stepSearch(canceled,5);canceled=nil;assert(C.fen(p)==original,'cancellation mutated displayed board')
  elseif f[1]=='END' then assert(idx==#moves,'extra legal moves '..n) end
 end
 p=nil;moves=nil;data=nil;collectgarbage('collect')
end
local start=assert(C.parseFen('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'))
assert(C.perft(start,3)==8902,'standard perft3')
print('source oracle positions: '..total)
