-- Private app-local slots; never load/import other apps' saves.
local S={}
local function checksum(s)
  local n=1;for i=1,#s do n=(n*257+s:byte(i))%65521 end;return n
end
function S.open(validate)
  local s={seq=0,slot=2}
  for slot=1,2 do
    local raw=smudge.read_file('connections-state'..slot..'.cache',1024)
    if type(raw)=='string' then
      local seq,hash,body=raw:match('^CP4\n(%d+)\n(%d+)\n(.*)$');seq=tonumber(seq)
      if seq and seq>0 and seq<=1000000 and checksum(seq..'\n'..body)==tonumber(hash) and validate(body) and seq>s.seq then
        s.seq,s.slot,s.body=seq,slot,body
      end
    end
  end
  function s:write(body)
    if self.seq>=1000000 or #body>512 or not validate(body) then return false end
    local seq,slot=self.seq+1,3-self.slot
    local raw='CP4\n'..seq..'\n'..checksum(seq..'\n'..body)..'\n'..body
    local path='connections-state'..slot..'.cache'
    if not smudge.write_file(path,raw) or smudge.read_file(path,1024)~=raw then return false end
    self.seq,self.slot,self.body=seq,slot,body;return true
  end
  return s
end
return S
