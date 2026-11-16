-- UI-only two-slot checked records; game snapshots remain unchanged.
local R={played=0,won=0,best=0,match='',seq=0,slot=0};local paths={'ui-record-a.dat','ui-record-b.dat'}
local function check(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function decode(raw)
 if type(raw)~='string'or #raw>256 then return end
 local seq,sum,body=raw:match('^UI1:(%d+):(%d+)\n(.*)$');seq,sum=tonumber(seq),tonumber(sum)
 if not seq or seq<1 or seq>1000000 or check(body or'')~=sum then return end
 local played,won,best,id=body:match('^(%d+) (%d+) (%d+)\n([%d:%-]+)$');played,won,best=tonumber(played),tonumber(won),tonumber(best)
 if not played or played>65535 or not won or won>played or not best or best>4 or not id or #id>100 or not id:match('^%-?%d+:%d+:%d+:%d+:%d+:%d+$')then return end
 return seq,played,won,best,id
end
function R.load()
 for slot,path in ipairs(paths)do local seq,played,won,best,id=decode(smudge.read_file(path,257))
  if seq and seq>R.seq then R.seq,R.slot,R.played,R.won,R.best,R.match=seq,slot,played,won,best,id end
 end
end
function R.observe(M,g)
 if g.phase~=2 then return end
 local id=table.concat({g.seed,g.round,g.seals[1],g.seals[2],M.score(g,1),M.score(g,2)},':');local place=g.seals[1]>=2 and 1 or 2
 if id==R.match then return end
 R.played=math.min(65535,R.played+1);R.won=math.min(65535,R.won+(place==1 and 1 or 0));R.best=R.best==0 and place or math.min(R.best,place);R.match=id;R.dirty=true
end
function R.save()
 if not R.dirty then return true end;if R.seq>=1000000 then return false end
 local body=R.played..' '..R.won..' '..R.best..'\n'..R.match;local seq=R.seq+1;local slot=R.slot==1 and 2 or 1
 local raw='UI1:'..seq..':'..check(body)..'\n'..body;local path=paths[slot]
 if not smudge.write_file(path,raw)or smudge.read_file(path,257)~=raw or not decode(raw)then return false end
 R.seq,R.slot,R.dirty=seq,slot,false;return true
end
return R
