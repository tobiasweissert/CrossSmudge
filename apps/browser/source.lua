-- Bounded random reads over an immutable native resource or checked SD object.
-- Tokenizer passes retain one 8 KiB cache, never a whole response string.
local Source={};Source.__index=Source;Source.__len=function(s)return s.size end
function Source.new(size,read)return setmetatable({size=size,read=read,base=-1,cache=''},Source)end
function Source:chunk(pos)
 local base=((pos-1)//8192)*8192
 if self.base~=base then
  local n=math.min(8192,self.size-base);local s=self.read(base,n)
  if type(s)~='string'or #s~=n then error('resource_read')end
  self.base,self.cache=base,s
 end
 return self.cache,self.base
end
function Source:sub(first,last)
 first=math.max(1,first or 1);last=math.min(last or self.size,self.size)
 if first>last then return ''end
 if last-first+1>24576 then error('source_slice_limit')end
 local cached,base=self:chunk(first)
 if last<=base+#cached then return cached:sub(first-base,last-base)end
 local parts={}
 while first<=last do local s,base=self:chunk(first);local finish=math.min(last,base+#s);parts[#parts+1]=s:sub(first-base,finish-base);first=finish+1 end
 return table.concat(parts)
end
function Source:find(pattern,first,plain)
 local pos=math.max(1,first or 1);local tail=''
 -- Plain bounded delimiters can search the resident bytes in place. Only
 -- materialize the tiny boundary overlap after an unsuccessful block scan.
 -- Keep the general-pattern/long-literal path's existing overlap semantics.
 if plain and #pattern>0 and #pattern<=128 then
  local overlap=#pattern-1
  while pos<=self.size do
   local chunk,base=self:chunk(pos);local offset=pos-base
   if #tail>0 then
    local boundary=tail..chunk:sub(offset,offset+overlap-1)
    local a,b=boundary:find(pattern,1,true)
    if a then return pos-#tail+a-1,pos-#tail+b-1 end
   end
   local a,b=chunk:find(pattern,offset,true)
   if a then return base+a,base+b end
   if base+#chunk==self.size then break end
   tail=overlap>0 and chunk:sub(math.max(offset,#chunk-overlap+1),#chunk)or ''
   pos=base+#chunk+1
  end
  return
 end
 while pos<=self.size do
  -- Search the resident block before requesting its successor. A token near
  -- the start must not evict this block merely to look 8 KiB ahead.
  local chunk,base=self:chunk(pos);local offset=pos-base
  local s=tail..chunk:sub(offset);local origin=pos-#tail
  local a,b=s:find(pattern,1,plain)
  if a then return origin+a-1,origin+b-1 end
  if base+#chunk==self.size then break end
  -- Closing raw tags/comments may straddle a block. Retain bounded overlap,
  -- rather than rereading the preceding SD block on every search.
  tail=s:sub(-128);pos=base+#chunk+1
 end
end
-- Quote-aware scans use bounded native string searches, not one Lua sub call
-- per byte of publisher JSON/attributes. No payload accumulation is needed.
function Source:tagEnd(pos)
 local quote
 while pos<=self.size do
  local chunk,base=self:chunk(pos);local i=pos-base
  local a=chunk:find(quote or [=[["'>]]=],i,quote~=nil)
  if not a then pos=base+#chunk+1
  else
   local c=chunk:sub(a,a);pos=base+a+1
   if quote then quote=nil elseif c=='>'then return pos-1 else quote=c end
  end
 end
 return self.size+1
end
function Source:skipSpace(pos)
 while pos<=self.size do
  local chunk,base=self:chunk(pos);local a=chunk:find('%S',pos-base)
  if a then return base+a end;pos=base+#chunk+1
 end
 return self.size+1
end
function Source:rawClose(name,pos)
 while pos<=self.size do
  local a=self:find('</',pos,true);if not a then return end
  local p=self:skipSpace(a+2)
  if self:sub(p,p+#name-1):lower()==name then
   p=self:skipSpace(p+#name)
   if self:sub(p,p)=='>'then return a,p end
  end
  pos=a+2
 end
end
function Source:valid()
 local carry=''
 for pos=1,self.size,8192 do
  local bytes=carry..self:sub(pos,math.min(self.size,pos+8191));if bytes:find('%z')then return false end
  local n,bad=utf8.len(bytes);carry=''
  if not n then
   if #bytes-bad>3 or bytes:byte(bad)<194 then return false end
   carry=bytes:sub(bad)
  end
 end
 return carry==''
end
return Source
