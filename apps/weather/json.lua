-- Bounded RFC 8259 decoder. No eval, metatables, or external modules.
local J = {null = {}}
function J.decode(s)
  if type(s)~='string' or #s>8192 then return nil,'body size' end
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
    if nodes>1024 or depth>12 then fail() end
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
