-- Local UTF-8 validation/encoding: firmware does not expose the utf8 library.
local utf8={}
function utf8.char(c)
  if c<128 then return string.char(c) end
  if c<2048 then return string.char(192+c//64,128+c%64) end
  if c<65536 then return string.char(224+c//4096,128+(c//64)%64,128+c%64) end
  return string.char(240+c//262144,128+(c//4096)%64,128+(c//64)%64,128+c%64)
end
function utf8.len(s)
  local i,n=1,0
  while i<=#s do
    local b=s:byte(i);local k,cp,minimum
    if b<128 then k=0;cp=b;minimum=0
    elseif b>=194 and b<=223 then k=1;cp=b-192;minimum=128
    elseif b>=224 and b<=239 then k=2;cp=b-224;minimum=2048
    elseif b>=240 and b<=244 then k=3;cp=b-240;minimum=65536
    else return nil end
    for j=1,k do local t=s:byte(i+j);if not t or t<128 or t>191 then return nil end;cp=cp*64+t-128 end
    if cp<minimum or cp>0x10ffff or (cp>=0xd800 and cp<=0xdfff) then return nil end
    i=i+k+1;n=n+1
  end
  return n
end
-- Bounded RFC 8259 decoder. No eval, metatables, or external modules.
local J = {null = {}}
function J.decode(s)
  if type(s)~='string' or #s>4096 then return nil,'body size' end
  local p,nodes=1,0
  local function fail() error('invalid JSON',0) end
  local function ws() local _,e=s:find('^[ \n\r\t]*',p); p=(e or p-1)+1 end
  local function str()
    p=p+1
    local parts,start={},p
    while p<=#s do
      local b=s:byte(p)
      if b==34 or b==92 then
        parts[#parts+1]=s:sub(start,p-1)
        if b==34 then
          p=p+1
          local v=table.concat(parts)
          if not utf8.len(v) then fail() end
          return v
        end
        p=p+1
        local c=s:sub(p,p)
        local escapes={['"']='"',['\\']='\\',['/']='/',b='\b',f='\f',n='\n',r='\r',t='\t'}
        if c=='u' then
          local function hex()
            local h=s:sub(p+1,p+4)
            if #h~=4 or h:find('[^%x]') then fail() end
            p=p+5
            return tonumber(h,16)
          end
          local cp=hex()
          if cp>=0xD800 and cp<=0xDBFF then
            if s:sub(p,p+1)~='\\u' then fail() end
            p=p+1
            local low=hex()
            if low<0xDC00 or low>0xDFFF then fail() end
            cp=0x10000+(cp-0xD800)*1024+low-0xDC00
          elseif cp>=0xDC00 and cp<=0xDFFF then fail() end
          parts[#parts+1]=utf8.char(cp)
        else
          if not escapes[c] then fail() end
          parts[#parts+1]=escapes[c]; p=p+1
        end
        start=p
      elseif b<32 then fail()
      else p=p+1 end
    end
    fail()
  end
  local value
  value=function(depth)
    nodes=nodes+1
    if nodes>256 or depth>8 then fail() end
    ws()
    local c=s:sub(p,p)
    if c=='"' then return str() end
    if c=='[' or c=='{' then
      p=p+1; ws()
      local t,seen={},{}
      local close=c=='[' and ']' or '}'
      if s:sub(p,p)==close then p=p+1; return t end
      while true do
        local k
        if c=='{' then
          if s:sub(p,p)~='"' then fail() end
          k=str(); ws()
          if seen[k] or s:sub(p,p)~=':' then fail() end
          seen[k]=true; p=p+1
        else k=#t+1 end
        t[k]=value(depth+1); ws()
        local nextc=s:sub(p,p); p=p+1
        if nextc==close then return t end
        if nextc~=',' then fail() end
        ws()
      end
    end
    for word,v in pairs({['true']=true,['false']=false,['null']=J.null}) do
      if s:sub(p,p+#word-1)==word then p=p+#word; return v end
    end
    local start=p
    if c=='-' then p=p+1 end
    if s:sub(p,p)=='0' then p=p+1
    else
      local _,e=s:find('^[1-9][0-9]*',p)
      if not e then fail() end
      p=e+1
    end
    local integral=true
    if s:sub(p,p)=='.' then
      integral=false; p=p+1
      local _,e=s:find('^[0-9]+',p); if not e then fail() end; p=e+1
    end
    if s:sub(p,p):match('[eE]') then
      integral=false; p=p+1
      if s:sub(p,p):match('[+-]') then p=p+1 end
      local _,e=s:find('^[0-9]+',p); if not e then fail() end; p=e+1
    end
    local raw=s:sub(start,p-1)
    -- Check decimal magnitude as text, before float32 can round 2147483647 up.
    if integral then
      local mag=raw:gsub('^-','')
      local bound=raw:sub(1,1)=='-' and '2147483648' or '2147483647'
      if #mag>10 or (#mag==10 and mag>bound) then fail() end
    end
    local v=tonumber(raw)
    if not v or v~=v or v==math.huge or v==-math.huge then fail() end
    return v
  end
  local ok,v=pcall(value,0)
  if not ok then return nil,'invalid JSON' end
  ws(); if p<=#s then return nil,'trailing JSON' end
  return v
end
return J
