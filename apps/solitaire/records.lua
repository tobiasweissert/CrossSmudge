-- UI record only: preserve the game journal; never invent pre-update history.
-- The original keeps the last 1024 results and displays the newest sixteen.
local R={results='',last='',played=0,wins=0,streak=0,recent={}}
local sequence,slot=0,1
local paths={'/.smudge/saved_data/solitaire/ui-records-1.dat','/.smudge/saved_data/solitaire/ui-records-2.dat'}
local function checksum(s)local a,b=1,0;for i=1,#s do a=(a+s:byte(i))%65521;b=(b+a)%65521 end;return a..':'..b end
local function refresh()
 R.played=#R.results;R.wins=0;R.streak=0;R.recent={}
 for i=1,#R.results do if R.results:sub(i,i)=='2'then R.wins=R.wins+1 end end
 for i=#R.results,1,-1 do if R.results:sub(i,i)~='2'then break end;R.streak=R.streak+1 end
 for i=1,16 do local at=#R.results-16+i;R.recent[i]=at>0 and tonumber(R.results:sub(at,at))or 0 end
end
local function decode(raw,C)
 if type(raw)~='string'or #raw>1264 then return end
 local seq,sum,body=raw:match('^UI1 (%d+) (%d+:%d+)\n(.*)$');seq=tonumber(seq)
 if not seq or math.type(seq)~='integer'or seq<1 or seq>1000000000 or #body>1200 or checksum(seq..' '..body)~=sum then return end
 local results,last=body:match('^([12]*)|(.*)$')
 if not results or #results>1024 or not(last==''or C.restore(last)~=nil)then return end
 return seq,results,last
end
function R.init(J,C)
 for index,path in ipairs(paths)do
  local seq,results,last=decode(smudge.read_file(path,1265),C)
  if seq and seq>sequence then sequence,slot,R.results,R.last=seq,index,results,last end
 end
 refresh()
end

function R.observe(C,g,won)
 local signature=C.save(g);if signature==R.last then return end
 R.results=(R.results..(won and '2'or'1')):sub(-1024);R.last=signature;R.dirty=true;refresh()
end
function R.begin()R.last='';R.dirty=true end
function R.save()
 if not R.dirty then return true end
 if sequence>=1000000000 then smudge.log('Solitaire UI record sequence limit reached');return false end
 local seq=sequence+1;local nextSlot=3-slot;local body=R.results..'|'..R.last;local raw='UI1 '..seq..' '..checksum(seq..' '..body)..'\n'..body
 if not smudge.write_file(paths[nextSlot],raw,false)or smudge.read_file(paths[nextSlot],#raw+1)~=raw then smudge.log('Solitaire UI record save failed');return false end
 sequence,slot,R.dirty=seq,nextSlot,false;return true
end
return R
