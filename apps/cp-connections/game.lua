-- Core rules adapted from CrossPlay ConnectionsCore (MIT). No puzzle bank.
local G={}
local function group(tile) return (tile-1)//4+1 end
local function identity(p)
  local h=-2128831035 -- FNV-1a uint32 offset, expressed in firmware int32 range
  local function feed(s)
    for i=1,#s do h=(h~s:byte(i))*16777619 end
    h=(h~0)*16777619
  end
  for _,g in ipairs(p.answers) do
    feed(g.group);feed(tostring(g.level));for _,w in ipairs(g.members) do feed(w) end
  end
  return string.format('%08x',h)
end
function G.new(p,seed)
  local g={puzzle=p,fingerprint=identity(p),order={},chosen={},history={},solved={},mistakes=0,result='playing'}
  for i=1,16 do g.order[i]=i end
  function g:shuffle(seed)
    local state=seed
    for i=#self.order,2,-1 do
      state=state*1664525+1013904223
      -- int32 Lua: compute uint32 remainder without float rounding.
      local j=(((state>>1)%i)*2+(state&1))%i+1
      self.order[i],self.order[j]=self.order[j],self.order[i]
    end
    self.chosen={}
  end
  function g:toggle(i)
    if self.result~='playing' or not self.order[i] then return end
    local tile=self.order[i]
    if self.chosen[tile] then self.chosen[tile]=nil;return end
    local n=0;for _ in pairs(self.chosen) do n=n+1 end
    if n<4 then self.chosen[tile]=true end
  end
  function g:submit()
    if self.result~='playing' then return 'invalid' end
    local mask,n,count=0,0,{0,0,0,0}
    for tile in pairs(self.chosen) do n=n+1;mask=mask|(1<<(tile-1));count[group(tile)]=count[group(tile)]+1 end
    if n~=4 then return 'choose four' end
    for _,old in ipairs(self.history) do if mask==old then return 'already guessed' end end
    self.history[#self.history+1]=mask
    local best,bg=0,1;for i=1,4 do if count[i]>best then best,bg=count[i],i end end
    if best==4 then
      self.solved[#self.solved+1]=bg
      local kept={};for _,tile in ipairs(self.order) do if group(tile)~=bg then kept[#kept+1]=tile end end
      self.order=kept;self.chosen={}
      if #self.solved==4 then self.result='won' end
      return 'solved'
    end
    self.mistakes=self.mistakes+1
    if self.mistakes==4 then self.result='lost' end
    return best==3 and 'one away' or 'wrong'
  end
  function g:encode()
    return self.puzzle.date..'|'..self.puzzle.id..'|'..self.fingerprint..'|'..table.concat(self.history,',')..'|'..table.concat(self.order,',')
  end
  g:shuffle(seed or 0)
  return g
end
function G.restore(p,raw)
  if type(raw)~='string' or #raw>512 then return end
  local date,id,fingerprint,hist,order=raw:match('^([^|]+)|(%d+)|(%x+)|([%d,]*)|([%d,]*)$')
  if date~=p.date or tonumber(id)~=p.id or fingerprint~=identity(p) then return end
  local g=G.new(p,0)
  for n in hist:gmatch('%d+') do
    local mask=tonumber(n);if mask<1 or mask>65535 or #g.history>=7 then return end
    g.chosen={}
    for _,t in ipairs(g.order) do if mask&(1<<(t-1))~=0 then g.chosen[t]=true end end
    local old=#g.history;g:submit();if #g.history~=old+1 or g.history[#g.history]~=mask then return end
  end
  local remaining={};for _,t in ipairs(g.order) do remaining[t]=true end
  local restored={}
  for n in order:gmatch('%d+') do local t=tonumber(n);if not remaining[t] then return end;remaining[t]=nil;restored[#restored+1]=t end
  if next(remaining) then return end
  g.order=restored;g.chosen={}
  -- Canonical round-trip rejects ambiguous comma sequences and modified history.
  if g:encode()~=raw then return end
  return g
end
return G
