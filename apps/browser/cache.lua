-- B5 checked index + immutable objects. No rename; B1 Jina journals untouched.
local Source=dofile('source.lua')
local C={}
local function sum(s,a)a=a or 1;for i=1,#s do a=(a*257+s:byte(i))%65521 end;return a end
C.sum=sum
local function pack(n,s)return 'B5:'..n..':'..#s..':'..sum(s)..'\n'..s end
local function unpacked(raw,limit)
 if type(raw)~='string'or #raw>limit+80 then return end
 local n,len,hash,body=raw:match('^B5:(%d+):(%d+):(%d+)\n(.*)$');n,len,hash=tonumber(n),tonumber(len),tonumber(hash)
 if n and n>0 and n<1000000 and len and #body==len and len<=limit and sum(body)==hash then return n,body end
end
function C.open()
 local root=smudge.browser_path and smudge.browser_path('')or ''
 local function path(n)return root..n end
 local c={seq=0,slot=1,entries={},bookmarks={},sources={},root=root}
 local function decode(s)
  local result={entries={},bookmarks={},sources={}}
  for line in s:gmatch('[^\n]+')do
   local kind,url,file,size,hash,page=line:match('^([EBS])\t([^\t]+)\t([^\t]*)\t(%d+)\t(%d+)\t(%d+)$')
   if not kind or not url:match('^https://')or #url>512 then return end
   if kind=='E'then
    if not file:match('^b5%-%d+%.page$')or #result.entries>=8 then return end
    result.entries[#result.entries+1]={url=url,file=file,size=tonumber(size),hash=tonumber(hash),page=tonumber(page)}
   elseif kind=='B'then if #result.bookmarks<20 then result.bookmarks[#result.bookmarks+1]=url end
   elseif kind=='S'then if #result.sources<8 then result.sources[#result.sources+1]=url end end
  end
  return result
 end
 local function encode(entries)
  local lines={}
  for _,e in ipairs(entries or c.entries)do lines[#lines+1]=table.concat({'E',e.url,e.file,e.size,e.hash,e.page or 1},'\t')end
  for _,e in ipairs(c.bookmarks)do lines[#lines+1]='B\t'..e..'\t\t0\t0\t0'end
  for _,e in ipairs(c.sources)do lines[#lines+1]='S\t'..e..'\t\t0\t0\t0'end
  return table.concat(lines,'\n')
 end
 for i=1,2 do
  local n,s=unpacked(smudge.read_file(path('b5-index'..i),24657),24576)
  local value=s and decode(s)
  if n and value and n>c.seq then c.seq,c.slot=n,i;c.entries=value.entries;c.bookmarks=value.bookmarks;c.sources=value.sources end
 end
 function c:commit(entries)
  if self.seq>=999999 then return false end
  local body=encode(entries);if #body>24576 then return false end
  local s=pack(self.seq+1,body);local slot=3-self.slot;local file=path('b5-index'..slot)
  if not smudge.write_file(file,s)or smudge.read_file(file,#s+1)~=s then return false end
  self.seq,self.slot=self.seq+1,slot;if entries then self.entries=entries end;return true
 end
 function c:lookup(url)for _,e in ipairs(self.entries)do if e.url==url then return e end end end
 function c:read(e)
  if not e or e.size>1048576 then return end
  local s=smudge.read_file(path(e.file),e.size+1)
  if s and #s==e.size and sum(s)==e.hash then return s end
 end
 function c:save(url,body,css,resource)
  local length=resource and resource.size or #body
  if length>1048576 or #url>512 then return end
  local id=self.seq+1;local name
  -- Existence checks avoid reusing an orphan left by interruption or failed I/O.
  for _=1,128 do name='b5-'..id..'.page';if not smudge.file_exists(path(name))then break end;id=id+1;name=nil end
  if not name then return end
  local file=path(name)
  local hash=resource and resource.hash or sum(body)
  if resource then
   if not smudge.http_store(resource.handle,name)then return end
  else
   if not smudge.write_file(file,'')then return end
   for p=1,#body,4096 do if not smudge.write_file(file,body:sub(p,p+4095),true)then return end end
   local verified=1
   for p=0,#body-1,4096 do local bytes=smudge.read_file(file,math.min(4096,#body-p),p);if not bytes or #bytes~=math.min(4096,#body-p)then return end;verified=sum(bytes,verified)end
   if verified~=hash or smudge.read_file(file,1,#body)~=''then return end
  end
  local e={url=url,file=name,size=length,hash=hash,page=1}
  if css and #css>0 then
   local wrapped=pack(1,css:sub(1,24576))
   if not smudge.write_file(file..'.css',wrapped)or smudge.read_file(file..'.css',#wrapped+1)~=wrapped then return end
  end
  local candidate={e}
  for _,entry in ipairs(self.entries)do if entry.url~=url and #candidate<8 then candidate[#candidate+1]=entry end end
  if not self:commit(candidate)then return end
  return e
 end
 function c:source(e)
  if not e or e.size>1048576 then return end
  local verified=1
  for offset=0,e.size-1,4096 do
   local count=math.min(4096,e.size-offset);local bytes=smudge.read_file(path(e.file),count,offset)
   if not bytes or #bytes~=count then return end;verified=sum(bytes,verified)
  end
  if verified~=e.hash or smudge.read_file(path(e.file),1,e.size)~=''then return end
  return Source.new(e.size,function(offset,count)return smudge.read_file(path(e.file),count,offset)end)
 end
 function c:css(e)
  if not e then return end
  local _,css=unpacked(smudge.read_file(path(e.file)..'.css',24657),24576);return css
 end
 function c:clearImages()
  if not smudge.list_files or not smudge.delete_file then return end
  local names=smudge.list_files(root:gsub('/$',''))
  for i=1,math.min(256,#names)do if names[i]:match('^b5%-image%d+%.jpg$')or names[i]:match('^b5%-image%d+%.png$')or names[i]:match('^b5%-image%d+%.img$')then smudge.delete_file(path(names[i]))end end
 end
 function c:cleanup()
  if not smudge.list_files or not smudge.delete_file then return end
  local live={}
  for i=1,2 do local _,s=unpacked(smudge.read_file(path('b5-index'..i),24657),24576);local value=s and decode(s)
   if value then for _,e in ipairs(value.entries)do live[e.file]=true end end
  end
  local names=smudge.list_files(root:gsub('/$',''))
  for i=1,math.min(256,#names)do local n=names[i];local object=n:match('^(b5%-%d+%.page)')
   if object and (n==object or n==object..'.css')and not live[object]then smudge.delete_file(path(n))end end
 end
 return c
end
return C
