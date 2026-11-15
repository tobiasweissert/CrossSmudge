-- Two checked slots: an interrupted write never truncates the previous slot.
-- Fixed names only; no provider-controlled paths. Checksum detects accidental
-- corruption, not malicious modification of SD contents.
local C={}
local function sum(s)
  local a=1
  for i=1,#s do a=(a*257+s:byte(i))%65521 end
  return a
end
local function decode(raw,limit)
  limit=limit or 8203
  if type(raw)~='string' or #raw>limit+200 then return end
  local seq,stamp,len,hash,body=raw:match('^CP1\n(%d+)\n([^\n]+)\n(%d+)\n(%d+)\n(.*)$')
  seq,len,hash=tonumber(seq),tonumber(len),tonumber(hash)
  if not seq or seq<1 or seq>1000000 or not len or len>limit or #body~=len or not stamp:match('^%d%d%d%d%-%d%d%-%d%d %d%d:%d%d local$') or sum(seq..'\n'..stamp..'\n'..len..'\n'..body)~=hash then return end
  return {seq=seq,stamp=stamp,body=body}
end
function C.open(name,validate,limit)
  assert(name:match('^[a-z]+%d*$'))
  limit=limit or 8203
  assert(limit>=1 and limit<=65556)
  local c={name=name,validate=validate,seq=0,slot=1}
  for slot=1,2 do
    local rec=decode(smudge.read_file(name..slot..'.cache',limit+201),limit)
    if rec and rec.seq>c.seq and validate(rec.body) then
      c.seq,c.slot,c.body,c.stamp=rec.seq,slot,rec.body,rec.stamp
    end
  end
  function c:save(body)
    if #body>limit or not self.validate(body) or self.seq>=1000000 then return false end
    local d=smudge.get_date()
    local stamp=string.format('%04d-%02d-%02d %02d:%02d local',d.year,d.month,d.day,d.hour,d.min)
    local seq,slot=self.seq+1,3-self.slot
    local raw='CP1\n'..seq..'\n'..stamp..'\n'..#body..'\n'..sum(seq..'\n'..stamp..'\n'..#body..'\n'..body)..'\n'..body
    local path=self.name..slot..'.cache'
    if not smudge.write_file(path,raw) then return false end
    local back=smudge.read_file(path,limit+201)
    if back~=raw or not decode(back,limit) then return false end
    self.seq,self.slot,self.body,self.stamp=seq,slot,body,stamp
    return true
  end
  return c
end
return C
