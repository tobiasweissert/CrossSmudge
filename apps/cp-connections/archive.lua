-- Incremental top-level object scanner. Only one <=4096-byte record is decoded.
-- Caller supplies bounded read-at and yields after each 1024 bytes, keeping UI responsive.
local A={}
local function date(s)
  if type(s)~='string' or not s:match('^%d%d%d%d%-%d%d%-%d%d$') then return false end
  local y,m,d=tonumber(s:sub(1,4)),tonumber(s:sub(6,7)),tonumber(s:sub(9,10))
  if y<2023 or y>2100 or m<1 or m>12 then return false end
  local days={31,(y%4==0 and (y%100~=0 or y%400==0)) and 29 or 28,31,30,31,30,31,31,30,31,30,31}
  return d>=1 and d<=days[m]
end
function A.valid(p)
  if type(p)~='table' or not date(p.date) or type(p.id)~='number' or p.id%1~=0 or p.id<0 or p.id>65535 or type(p.answers)~='table' or #p.answers~=4 then return false end
  local seen,playable={},true
  for _,g in ipairs(p.answers) do
    if type(g)~='table' or type(g.group)~='string' or #g.group<1 or #g.group>160 or g.group:find('%c') or type(g.level)~='number' or g.level%1~=0 or g.level< -1 or g.level>3 or type(g.members)~='table' or #g.members~=4 then return false end
    for _,w in ipairs(g.members) do
      if type(w)~='string' or #w>80 or w:find('%c') or seen[w] then return false end
      if #w==0 then playable=false else seen[w]=true end
    end
  end
  return true,playable
end
function A.new(read,decode,target)
  local invalid=target~=nil and not date(target)
  local s={offset=0,total=nil,phase='start',depth=0,quote=false,escape=false,record='',best=nil,records=0,done=invalid,error=invalid and 'invalid device date' or nil}
  function s:step()
    if self.done then return true,self.best,self.error end
    local function fail(e) self.done=true;self.error=e;self.best=nil;return true,nil,e end
    local chunk,total=read(self.offset,1024)
    if not chunk then return fail(total) end
    if type(total)~='number' or total<0 or total>2097152 or #chunk>1024 or (self.total and total~=self.total) then return fail('archive changed') end
    self.total=total
    if #chunk==0 then
      if self.offset~=total or self.phase~='end' then return fail('truncated archive') end
      self.done=true
      if not self.best then self.error='date unavailable' end
      return true,self.best,self.error
    end
    local begin=1
    for i=1,#chunk do
      local c=chunk:sub(i,i)
      if self.phase=='object' then
        if self.quote then
          if self.escape then self.escape=false elseif c=='\\' then self.escape=true elseif c=='"' then self.quote=false end
        elseif c=='"' then self.quote=true
        elseif c=='{' or c=='[' then self.depth=self.depth+1;if self.depth>8 then return fail('nesting limit') end
        elseif c=='}' or c==']' then self.depth=self.depth-1 end
        if #self.record+i-begin+1>4096 then return fail('record limit') end
        if self.depth==0 then
          self.record=self.record..chunk:sub(begin,i)
          local p=decode(self.record);self.record=''
          local valid,playable=A.valid(p)
          if not valid then return fail('invalid puzzle') end
          self.records=self.records+1
          if playable and ((target and p.date==target) or (not target and (not self.best or p.date>self.best.date))) then
            if self.best and p.date==self.best.date then return fail('duplicate date') end
            self.best=p
          end
          self.phase='separator'
        end
      elseif c:match('%s') then
        if c~=' ' and c~='\n' and c~='\r' and c~='\t' then return fail('invalid whitespace') end
      elseif self.phase=='start' and c=='[' then self.phase='first'
      elseif (self.phase=='first' or self.phase=='next') and c=='{' then self.phase='object';self.depth=1;begin=i
      elseif self.phase=='separator' and c==',' then self.phase='next'
      elseif (self.phase=='first' or self.phase=='separator') and c==']' then self.phase='end'
      else return fail('invalid archive') end
    end
    if self.phase=='object' then self.record=self.record..chunk:sub(begin) end
    self.offset=self.offset+#chunk
    if self.offset>total then return fail('archive changed') end
    return false
  end
  return s
end
return A
