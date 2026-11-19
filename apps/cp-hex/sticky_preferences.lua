-- UI-only seat/record hints, keyed to the unchanged core encoding. Write a new
-- hint before the core journal; the previous matching hint survives failed saves.
local P={nextMode=1,nextSeat=1,seat=1,wins=0,losses=0,last='',signature='',lastMove=0,pending=0,epoch=1,countedEpoch=0,maxEpoch=1,currentCounted=0}
local paths={'native-view-a.dat','native-view-b.dat'};local seq,slot,lastSaved=0,0,nil;local committedSlot,preparedSlot=0,0;local observed,armed
local function checksum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function parse(M,raw)
 if type(raw)~='string'or #raw>1600 then return end
 local n,sum,body=raw:match('^HX1:(%d+):(%d+)\n(.*)$');n,sum=tonumber(n),tonumber(sum)
 if not n or math.type(n)~='integer'or n<1 or n>1000000 or checksum(body or'')~=sum then return end
 local mode,seat,active,w,l,move,pending,epoch,countedEpoch,currentCounted,key,last=body:match('^([12]) ([12]) ([12]) (%d+) (%d+) (%d+) ([01]) (%d+) (%d+) ([01])\n([^\n]+)\n(.*)$')
 w,l,move,epoch,countedEpoch=tonumber(w),tonumber(l),tonumber(move),tonumber(epoch),tonumber(countedEpoch)
 if not epoch or not countedEpoch or math.type(epoch)~='integer'or math.type(countedEpoch)~='integer'or epoch<1 or epoch>1000000 or countedEpoch>1000000 then return end
 if not mode or not w or not l or not move or math.type(w)~='integer'or math.type(l)~='integer'or w>65535 or l>65535 or w+l>65535 or move>121 then return end
 local game=M.decode(key);if not game then return end
 if last~='-'then local final=M.decode(last);if not final or final.status==0 then return end end
 return {seq=n,mode=tonumber(mode),nextSeat=tonumber(seat),seat=tonumber(active),wins=w,losses=l,epoch=epoch,countedEpoch=countedEpoch,currentCounted=tonumber(currentCounted),pending=tonumber(pending),body=body,move=move,key=key,last=last=='-'and''or last}
end
function P.load(M,g,mode)
 P.nextMode=mode;local key=M.encode(g);local active=-1;local already=false
 for at,path in ipairs(paths)do local record=parse(M,smudge.read_file(path,1601))
  if record then
   P.maxEpoch=math.max(P.maxEpoch,record.epoch,record.countedEpoch)
   if record.seq>seq then seq,slot=record.seq,at;P.nextMode,P.nextSeat,P.wins,P.losses,P.last=record.mode,record.nextSeat,record.wins,record.losses,record.last;P.pending=record.pending;P.countedEpoch=record.countedEpoch;lastSaved=record.body end
   if record.key==key and record.seq>active then active=record.seq;P.seat,P.lastMove=record.seat,record.move;committedSlot=at;P.epoch=record.epoch;already=record.currentCounted==1 end
  end
 end
 P.currentCounted=(already or P.countedEpoch==P.epoch)and 1 or 0;P.signature=key;observed=g;armed=P.currentCounted==0
end
function P.observe(M,g,mode)
 if g~=observed then observed=g;armed=g.status==0;P.lastMove=0;P.pending=0 end
 if armed and g.status~=0 then
  armed=false;local final=M.encode(g)
  -- The running-to-complete transition identifies a new game. Hex has no
  -- RNG, so two genuine new games may finish with identical encoded boards.
  if P.wins+P.losses<65535 then if g.status==(mode==2 and 1 or P.seat)then P.wins=P.wins+1 else P.losses=P.losses+1 end end
  P.last=final;P.countedEpoch=P.epoch;P.currentCounted=1
 end
end
function P.begin(g)P.epoch=math.min(1000001,math.max(P.maxEpoch,P.epoch,P.countedEpoch)+1);P.maxEpoch=P.epoch;observed=g;armed=true;P.currentCounted=0;P.seat=P.nextMode==2 and 1 or P.nextSeat;P.lastMove=0;P.pending=0 end
function P.save(M,g)
 if seq>=1000000 or P.epoch>1000000 then smudge.log('Hex UI journal exhausted');return false end
 local body=table.concat({P.nextMode,P.nextSeat,P.seat,P.wins,P.losses,P.lastMove,P.pending,P.epoch,P.countedEpoch,P.currentCounted},' ')..'\n'..M.encode(g)..'\n'..(P.last==''and'-'or P.last)
 if body==lastSaved then preparedSlot=slot;return true end
 local nextSlot=committedSlot==1 and 2 or 1;local raw='HX1:'..(seq+1)..':'..checksum(body)..'\n'..body
 if not smudge.write_file(paths[nextSlot],raw)or smudge.read_file(paths[nextSlot],1601)~=raw then smudge.log('Hex UI hint save failed');return false end
 seq,slot,lastSaved,preparedSlot=seq+1,nextSlot,body,nextSlot;return true
end
function P.commit()committedSlot=preparedSlot end
return P
