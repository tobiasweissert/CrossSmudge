-- UI-only observations since this presentation was installed. Never reconstruct old history.
-- Separate read-back checked slots leave the original game journal and codec untouched.
local R={rounds=0,points=0,buckets={0,0,0,0,0},last='',seq=0,slot=0,abandoned=0}
local paths={'ui-observed-a.dat','ui-observed-b.dat'}
local function sum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
-- Completion identity excludes mode: changing next-round preference is not a new result.
local function completion(g)return table.concat({g.rng,g.seen,g.a,g.b,g.pair,g.target,g.guess,g.round,g.total,g.points,g.call},':')end
local function payload(r)return table.concat({r.rounds,r.points,table.concat(r.buckets,','),r.abandoned,r.last},'|')end
local function parse(raw)
 if type(raw)~='string'or#raw>4096 then return end
 local seq,check,body=raw:match('^WU1:(%d+):(%d+)\n(.*)$');seq,check=tonumber(seq),tonumber(check)
 if not seq or seq<1 or seq>1000000 or check~=sum(body or'')then return end
 local rounds,points,buckets,abandoned,last=body:match('^(%d+)|(%d+)|([%d,]+)|(%d+)|(.*)$');rounds,points,abandoned=tonumber(rounds),tonumber(points),tonumber(abandoned)
 if not rounds or rounds>65535 or points>65535 or abandoned>65535 then return end
 local b={};for n in buckets:gmatch('%d+')do n=tonumber(n);if n>65535 then return end;b[#b+1]=n end
 if#b~=5 then return end
 return{rounds=rounds,points=points,buckets=b,abandoned=abandoned,last=last,seq=seq}
end
function R.load(g,M)
 local best
 for slot=1,2 do local value=parse(smudge.read_file(paths[slot],4097));if value and(not best or value.seq>best.seq)then best=value;best.slot=slot end end
 if best then for k,v in pairs(best)do R[k]=v end else R.last=g.phase==5 and completion(g)or M.encode(g)end
 return R
end
function R.save(g,M,abandoned,begin)
 local signature=g.phase==5 and completion(g)or M.encode(g);local next={rounds=R.rounds,points=R.points,buckets={table.unpack(R.buckets)},abandoned=abandoned,last=begin and'BEGIN:'..signature or R.last}
 if not begin and g.phase==5 and g.round>1 and signature~=R.last then
  next.rounds=math.min(65535,next.rounds+1);next.points=math.min(65535,next.points+g.points);local d=math.abs(g.guess-g.target);local bucket=d<=2 and d+1 or d<=5 and 4 or 5;next.buckets[bucket]=math.min(65535,next.buckets[bucket]+1);next.last=signature
 end
 local body=payload(next);if body==payload(R)and R.seq>0 then return true end
 if R.seq>=1000000 then smudge.log('Wavelength UI observation save limit');return false end
 local slot=R.slot==1 and 2 or 1;local raw='WU1:'..(R.seq+1)..':'..sum(body)..'\n'..body
 if not smudge.write_file(paths[slot],raw)or smudge.read_file(paths[slot],4097)~=raw then smudge.log('Wavelength UI observation save failed');return false end
 next.seq=R.seq+1;next.slot=slot;for k,v in pairs(next)do R[k]=v end;return true
end
return R
