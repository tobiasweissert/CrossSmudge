-- One UI bit per category: a completed zero must read BEST 0, not NEW.
-- Separate from the unchanged game journal and committed only after it.
local R={mask=0,seq=0,slot=0,dirty=false,needsClear=false}
local paths={'ui-categories-a.dat','ui-categories-b.dat'}
local function checksum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function decode(raw)
 if type(raw)~='string'or #raw>96 then return end
 local seq,mask,rounds,sum=raw:match('^FC1 (%d+) (%d+) (%d+) (%d+)$')
 seq,mask,rounds,sum=tonumber(seq),tonumber(mask),tonumber(rounds),tonumber(sum)
 if not seq or seq<1 or seq>1000000 or not mask or mask>131071 or not rounds or rounds>65535 then return end
 if checksum(seq..' '..mask..' '..rounds)~=sum then return end
 return seq,mask,rounds
end
function R.init(g)
 for i,path in ipairs(paths)do local raw=smudge.read_file(path,97);local seq,mask,rounds=decode(raw)
  if raw and not seq then smudge.log('Forehead UI category journal rejected')end
  if seq and seq>R.seq then
   -- Preserve the validated envelope sequence even when Reset made its
   -- payload stale. Restarting at1 lets a surviving old slot outrank new data.
   R.seq,R.slot=seq,i
   R.mask=rounds<=g.rounds and mask or 0
   R.dirty=rounds>g.rounds;R.needsClear=R.dirty
  end
 end
 for i=1,17 do if tonumber(g.bestIn:sub((i-1)*3+1,i*3))>0 then R.mask=R.mask|(1<<(i-1))end end
end
function R.has(category)return R.mask&(1<<(category-1))~=0 end
function R.observe(g)
 if g.phase==3 and g.rounds>0 and #g.history>0 and not R.has(g.category)then R.mask=R.mask|(1<<(g.category-1));R.dirty=true end
end
function R.reset()R.mask=0;R.dirty=true;R.needsClear=true end
function R.save(g)
 R.observe(g);if not R.dirty then return true end
 if R.seq>=1000000 then smudge.log('Forehead UI category sequence exhausted');return false end
 local seq=R.seq+1;local slot=R.slot==1 and 2 or 1;local body=seq..' '..R.mask..' '..g.rounds;local raw='FC1 '..body..' '..checksum(body)
 if not smudge.write_file(paths[slot],raw)or smudge.read_file(paths[slot],97)~=raw then smudge.log('Forehead UI category save failed');return false end
 R.seq,R.slot,R.dirty=seq,slot,false;R.needsClear=false;return true
end
return R
