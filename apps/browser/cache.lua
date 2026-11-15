-- Two checked slots; no response-sized concatenation or readback buffer.
local C={}
local function sum(s,a)
 a=a or 1;for i=1,#s do a=(a*257+s:byte(i))%65521 end;return a
end
local function decode(raw)
 if not raw or #raw>8700 then return end
 local n,len,hash,body=raw:match('^B1:(%d+):(%d+):(%d+)\n(.*)$')
 n,len,hash=tonumber(n),tonumber(len),tonumber(hash)
 if n and n>=1 and n<=1000000 and len and len<=8500 and #body==len and sum(body,sum(n..':'..len..':'))==hash then return n,hash,body end
end
function C.open(name,validate)
 local c={seq=0,slot=1}
 local function path(slot)return name..slot..'.cache'end
 for slot=1,2 do
  collectgarbage('collect')
  local n,hash,body=decode(smudge.read_file(path(slot),8701))
  if n and n>c.seq and validate(body)then c.seq,c.slot=n,slot end
 end
 collectgarbage('collect')
 if c.seq>0 then
  local raw=smudge.read_file(path(c.slot),8701);local n,hash,body=decode(raw)
  if n then
   local split=body:find('\n',1,true);c.body=body
   c.offset,c.size=#raw-#body+split,#body-split
   c.hash,c.seed=hash,sum(body:sub(1,split),sum(n..':'..#body..':'))
  end
 end
 function c:save(body,lead)
  local length=#lead+#body;if length>8500 or self.seq>=1000000 then return false end
  local n,slot=self.seq+1,3-self.slot
  local seed=sum(lead,sum(n..':'..length..':'));local hash=sum(body,seed)
  local header='B1:'..n..':'..length..':'..hash..'\n'..lead
  if not smudge.write_file(path(slot),header)or not smudge.write_file(path(slot),body,true)then return false end
  if smudge.read_file(path(slot),#header)~=header then return false end
  for offset=0,#body-1,512 do
   if smudge.read_file(path(slot),math.min(512,#body-offset),#header+offset)~=body:sub(offset+1,offset+512)then return false end
  end
  if smudge.read_file(path(slot),1,#header+#body)~=''then return false end
  self.seq,self.slot,self.offset,self.size,self.hash,self.seed=n,slot,#header,#body,hash,seed
  return true
 end
 function c:read()
  local body=smudge.read_file(path(self.slot),8193,self.offset)
  if body and #body==self.size and sum(body,self.seed)==self.hash then return body end
 end
 return c
end
return C
