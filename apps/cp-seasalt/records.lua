-- Separate bounded record journal; gameplay snapshots keep their existing format.
local R={played=0,won=0,match='',seq=0,slot=0};local paths={'ui-record-a.dat','ui-record-b.dat'}
local function checksum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function decode(raw)
 if type(raw)~='string'or #raw>256 then return end
 local seq,sum,body=raw:match('^SS1:(%d+):(%d+)\n(.*)$');seq,sum=tonumber(seq),tonumber(sum)
 if not seq or seq<1 or seq>1000000 or checksum(body or'')~=sum then return end
 local played,won,id=body:match('^(%d+) (%d+)\n([%d:%-]+)$');played,won=tonumber(played),tonumber(won)
 if not played or played>65535 or not won or won>played or #id>100 or not id:match('^%-?%d+:%d+:%d+:%d+:%d+:%d+$') then return end
 return seq,played,won,id
end
function R.load()
 for slot,path in ipairs(paths)do local seq,played,won,id=decode(smudge.read_file(path,257))
  if seq and seq>R.seq then R.seq,R.slot,R.played,R.won,R.match=seq,slot,played,won,id end
 end
end
function R.observe(M,g)
 if g.phase~=3 then return end
 local id=g.seed..':'..g.round..':'..g.score[1]..':'..g.score[2]..':'..M.count(g,1,9,true)..':'..M.count(g,2,9,true)
 if id==R.match then return end
 local you=M.count(g,1,9,true)==4 or M.count(g,2,9,true)~=4 and(g.score[1]>g.score[2]or g.score[1]==g.score[2]and g.starter==2)
 R.played=math.min(65535,R.played+1);R.won=math.min(65535,R.won+(you and 1 or 0));R.match=id;R.dirty=true
end
function R.save()
 if not R.dirty then return true end;if R.seq>=1000000 then return false end
 local body=R.played..' '..R.won..'\n'..R.match;local seq=R.seq+1;local slot=R.slot==1 and 2 or 1
 local raw='SS1:'..seq..':'..checksum(body)..'\n'..body;local path=paths[slot]
 if not smudge.write_file(path,raw)or smudge.read_file(path,257)~=raw or not decode(raw)then return false end
 R.seq,R.slot,R.dirty=seq,slot,false;return true
end
return R
