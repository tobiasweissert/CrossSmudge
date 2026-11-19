-- App-private, readback-verified two-slot journal. No executable save data.
local J={}
local function checksum(s) local a,b=1,0;for i=1,#s do a=(a+s:byte(i))%65521;b=(b+a)%65521 end;return a..':'..b end
function J.open(id,limit,validate,tag)
 tag=tag or ""
 assert(tag:match("^[a-z0-9%-]*$") and #tag<=32)
 local base='/.smudge/saved_data/'..id..'/'
 local j={sequence=0,slot=1,base=base,limit=limit}
 local function decode(raw)
  if type(raw)~='string' or #raw>limit+64 then return nil end
  local seq,len,sum,at=raw:match('^CP1 (%d+) (%d+) (%d+:%d+)\n()')
  seq,len=tonumber(seq),tonumber(len)
  if not seq or seq<1 or seq>1000000000 or not len or len>limit then return nil end
  local s=raw:sub(at);if #s~=len or checksum(seq..' '..s)~=sum or not validate(s) then return nil end
  return seq,s
 end
 local chosen
 for slot=1,2 do local raw=smudge.read_file(base..'crossplay-'..tag..slot..'.dat',limit+65,0);local seq,s=decode(raw)
  if seq and seq>j.sequence then j.sequence=seq;j.slot=slot;chosen=s end
 end
 function j:write(s)
  if type(s)~='string' or #s>limit or not validate(s) or self.sequence>=1000000000 then return false end
  local seq=self.sequence+1;local slot=3-self.slot;local raw='CP1 '..seq..' '..#s..' '..checksum(seq..' '..s)..'\n'..s
  local path=base..'crossplay-'..tag..slot..'.dat'
  if not smudge.write_file(path,raw,false) or smudge.read_file(path,#raw+1,0)~=raw then return false end
  self.sequence=seq;self.slot=slot;return true
 end
 -- Legacy original is never written. Bounded backup is byte-for-byte and read back.
 function j:preserve(key,maxBytes)
  local s=smudge.read_file(base..key..'.dat',maxBytes+1,0)
  if not s or #s==0 then return false end
  if #s>maxBytes then return true,'Old save retained; too large to copy' end
  local path=base..'legacy-'..key..'.bak';local old=smudge.read_file(path,maxBytes+1,0)
  if old==nil then
   if not smudge.write_file(path,s,false) or smudge.read_file(path,maxBytes+1,0)~=s then return true,'Old save retained; backup failed' end
  elseif old~=s then return true,'Old save and prior backup retained' end
  return true,'Old save backed up; start a new game'
 end
 return j,chosen
end
return J
