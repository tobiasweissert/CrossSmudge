-- UI-only preferences, human seat and observed records. Core keys pair hints
-- with the unchanged game journal; pending hints never overwrite its last match.
local P={nextMode=1,nextSeat=1,seat=1,nextBoard=9,nextHandicap=0,pending=0,epoch=1,countedEpoch=0,currentCounted=0,maxEpoch=1,wins=0,losses=0,lastWon=0,margin=0,last=''}
local paths={'native-view-a.dat','native-view-b.dat'};local seq,slot,committed,prepared,lastSaved=0,0,0,0,nil;local observed
local function sum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function parse(M,raw)
 if type(raw)~='string'or #raw>4096 then return end
 local n,check,body=raw:match('^GO1:(%d+):(%d+)\n(.*)$');n,check=tonumber(n),tonumber(check)
 if not n or math.type(n)~='integer'or n<1 or n>1000000 or sum(body or'')~=check then return end
 local header,key,last=body:match('^([%d ]+)\n([^\n]+)\n(.*)$');if not header then return end;local v={}
 for token in header:gmatch('%d+')do local value=tonumber(token);if math.type(value)~='integer'or value>1000000 then return end;v[#v+1]=value end
 if #v~=13 or(v[1]~=1 and v[1]~=2)or(v[2]~=1 and v[2]~=2)or(v[3]~=1 and v[3]~=2)or(v[4]~=9 and v[4]~=13 and v[4]~=19)or(v[5]~=0 and(v[5]<2 or v[5]>5))or v[6]>1 or v[7]<1 or v[9]>1 or v[10]>65535 or v[11]>65535 or v[10]+v[11]>65535 or v[12]>1 or v[13]>2000 then return end
 if not M.decode(key)then return end
 if last~='-'then local g=M.decode(last);if not g or g.phase~=2 then return end end
 return {seq=n,v=v,key=key,last=last=='-'and''or last,body=body}
end
function P.load(M,g)
 P.nextMode=g.solo==1 and 1 or 2;P.nextBoard=g.n;P.nextHandicap=g.handicap;local key=M.encode(g);local active=-1;local already=false
 for at,path in ipairs(paths)do local r=parse(M,smudge.read_file(path,4097))
  if r then local v=r.v;P.maxEpoch=math.max(P.maxEpoch,v[7],v[8])
   if r.seq>seq then seq,slot,lastSaved=r.seq,at,r.body;P.nextMode,P.nextSeat,P.nextBoard,P.nextHandicap,P.pending=v[1],v[2],v[4],v[5],v[6];P.countedEpoch,P.wins,P.losses,P.lastWon,P.margin,P.last=v[8],v[10],v[11],v[12],v[13],r.last end
   if r.key==key and r.seq>active then active=r.seq;committed=at;P.seat,P.epoch=v[3],v[7];already=v[9]==1 end
  end
 end
 P.currentCounted=(already or P.countedEpoch==P.epoch)and 1 or 0;observed=g
end
function P.observe(M,g)
 if g.phase~=2 or P.currentCounted==1 then return end
 local black,white=M.score(g);local winner=black>white and 1 or 2;local won=winner==(g.solo==0 and 1 or P.seat)
 if P.wins+P.losses<65535 then if won then P.wins=P.wins+1 else P.losses=P.losses+1 end end
 P.lastWon=won and 1 or 0;P.margin=math.abs(black-white);P.last=M.encode(g);P.currentCounted=1;P.countedEpoch=P.epoch
end
function P.begin(g)
 P.epoch=math.min(1000001,math.max(P.maxEpoch,P.epoch,P.countedEpoch)+1);P.maxEpoch=P.epoch;P.currentCounted=0;P.pending=0;P.seat=P.nextMode==2 and 1 or P.nextHandicap>0 and 1 or P.nextSeat;observed=g
end
function P.save(M,g)
 if seq>=1000000 or P.epoch>1000000 then smudge.log('Go UI journal limit reached');return false end
 local body=table.concat({P.nextMode,P.nextSeat,P.seat,P.nextBoard,P.nextHandicap,P.pending,P.epoch,P.countedEpoch,P.currentCounted,P.wins,P.losses,P.lastWon,P.margin},' ')..'\n'..M.encode(g)..'\n'..(P.last==''and'-'or P.last)
 if #body>4000 then smudge.log('Go UI hint too large');return false end
 if body==lastSaved then prepared=slot;return true end
 local at=committed==1 and 2 or 1;local raw='GO1:'..(seq+1)..':'..sum(body)..'\n'..body
 if not smudge.write_file(paths[at],raw)or smudge.read_file(paths[at],4097)~=raw then smudge.log('Go UI hint save failed');return false end
 seq,slot,prepared,lastSaved=seq+1,at,at,body;return true
end
function P.commit()committed=prepared end
local cached,key
function P.history(M)if key~=P.last then key=P.last;cached=key~=''and M.decode(key)or nil end;return cached end
return P
