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
 local pos=first or 1
 while pos<=self.size do
  local last=math.min(self.size,pos+8191);local s=self:sub(pos,last)
  local a,b=s:find(pattern,1,plain)
  if a then return pos+a-1,pos+b-1 end
  if last==self.size then break end
  pos=last-127
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
