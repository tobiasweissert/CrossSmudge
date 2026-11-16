local M=dofile('core.lua');local g=M.new(42);local hearts=M.title=='Hearts'
if hearts then for i=1,8000 do if g.phase==4 then break elseif g.phase==0 then M.commit(g,{M.passpick(g,1),M.passpick(g,2),M.passpick(g,3),M.passpick(g,4)})elseif g.phase==1 then M.play(g,M.choose(g))elseif g.phase==2 then M.sweep(g)else g.handno=g.handno+1;M.deal(g)end end
else for i=1,1200 do if g.phase==2 then break elseif g.phase==1 then M.act(g,1)else M.apply(g,M.ai(g))end end end
assert(g.phase==(hearts and 4 or 2));local R=dofile('records.lua');R.load();R.observe(M,g);assert(R.played==1);assert(R.save());R.observe(M,g);assert(R.played==1)
local loaded=dofile('records.lua');loaded.load();assert(loaded.played==1 and loaded.best>=1);loaded.observe(M,g);assert(loaded.played==1)
loaded.dirty=true;assert(loaded.save());local path=loaded.slot==1 and'ui-record-a.dat'or'ui-record-b.dat';assert(smudge.write_file(path,'truncated'));local fallback=dofile('records.lua');fallback.load();assert(fallback.played==1)
local write=smudge.write_file;smudge.write_file=function()return false end;fallback.dirty=true;assert(not fallback.save());smudge.write_file=write;print('Records deduplicate completed matches, verify writes, recover earlier slot and report failed writes')
