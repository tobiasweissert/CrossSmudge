-- Separate B7 checked roots/immutable objects. Never touches B5/B1 files.
local J,P=dofile('json.lua'),dofile('protocol.lua')
local B={}
local function sum(s,a)a=a or 1;for i=1,#s do a=(a*257+s:byte(i))%65521 end;return a end
local function pack(seq,s)return 'B7:'..seq..':'..#s..':'..sum(s)..'\n'..s end
local function unpacked(s)
 if type(s)~='string'or #s>65536 then return end
 local seq,n,h,body=s:match('^B7:(%d+):(%d+):(%d+)\n(.*)$');seq,n,h=tonumber(seq),tonumber(n),tonumber(h)
 if seq and seq>0 and seq<1000000 and n==#body and n<=49152 and sum(body)==h then return seq,body end
end
function B.open(W)
 local root=smudge.browser_path('');local c={entries=J.array(),seq=0,slot=1,serial=0}
 local function read(name,n)return smudge.read_file(root..name,n+1)end
 local function index(raw)
  local seq,body=unpacked(raw);if not body then return end
  local ok,t=pcall(J.decode,body);if not ok or not J.isarray(t)or #t>8 then return end
  local seen={}
  for _,e in ipairs(t)do
   if type(e)~='table'or type(e.doc)~='string'or #e.doc~=32 or not e.doc:match('^[a-f0-9]+$')or seen[e.doc]or type(e.url)~='string'or #e.url>512 or type(e.title)~='string'or #e.title>200 or math.type(e.chunks)~='integer'or e.chunks<1 or e.chunks>64 or type(e.files)~='table'or type(e.images)~='table'then return end
   local allowed={doc=true,url=true,title=true,mode=true,chunks=true,files=true,images=true,b7=true}
   for k in pairs(e)do if not allowed[k]then return end end
   if e.mode~='reader'and e.mode~='simplified'or J.isarray(e.files)or J.isarray(e.images)or e.b7~=nil and e.b7~=true then return end
   seen[e.doc]=true
   for k,v in pairs(e.files)do local n=tonumber(k);if not n or math.type(n)~='integer'or n<0 or n>=e.chunks or type(v)~='table'or type(v.file)~='string'or not v.file:match('^b7%-%d+%.chunk$')or math.type(v.size)~='integer'or v.size<1 or v.size>49232 then return end end
   local count=0;for k,v in pairs(e.images)do count=count+1;if count>16 or type(k)~='string'or #k~=32 or not k:match('^[a-f0-9]+$')or type(v)~='table'or type(v.file)~='string'or not v.file:match('^b7%-%d+%.png$')or math.type(v.size)~='integer'or v.size<1 or v.size>65536 or math.type(v.hash)~='integer'then return end end
  end
  return seq,t
 end
 local function roots()
  local live={}
  for slot=1,2 do
   local name='b7-index'..slot;local raw=read(name,65535)
   if not raw and smudge.file_exists(root..name)then c.uncertain=true;return end
   local _,entries=index(raw)
   -- A present malformed root is also uncertain: never collect its objects.
   if raw and not entries then c.uncertain=true;return end
   for _,e in ipairs(entries or {})do
    for _,f in pairs(e.files)do if live[f.file]and live[f.file]~=f.size then c.uncertain=true;return end;live[f.file]=f.size end
    for _,v in pairs(e.images)do if live[v.file]and live[v.file]~=v.size then c.uncertain=true;return end;live[v.file]=v.size end
   end
  end
  return live
 end
 for slot=1,2 do local name='b7-index'..slot;local raw=read(name,65535);local seq,t=index(raw)
  if (not raw and smudge.file_exists(root..name))or(raw and not t)then c.uncertain=true end
  if seq and seq>c.seq then c.seq,c.slot,c.entries=seq,slot,t end
 end
 function c:commit(entries,publish)
  if self.uncertain or self.seq>=999999 then return nil,'publication_uncertain'end
  local body=J.encode(entries);if #body>49152 then return nil,'index_limit'end
  local raw=pack(self.seq+1,body);local slot=3-self.slot;local success=false;W.check()
  W.atomic(function()
   self.uncertain=true
   if smudge.write_file(root..'b7-index'..slot,raw)and read('b7-index'..slot,#raw)==raw then self.seq,self.slot,self.entries=self.seq+1,slot,entries;self.uncertain=nil;if publish then publish()end;success=true end
  end)
  return success,success and nil or 'publication_uncertain'
 end
 function c:lookup(doc)for _,e in ipairs(self.entries)do if e.doc==doc then return e end end end
 function c:coverage(e)local n=0;for _ in pairs(e.files)do n=n+1 end;return n,e.chunks end
 function c:load(e,chunk)
  local f=e and e.files[tostring(chunk)];if not f then return end
  local wrapped=read(f.file,65535);if not wrapped or #wrapped~=f.size then return end
  local _,body=unpacked(wrapped);if not body then return end
  local ok,doc=pcall(P.decode,body,200)
  if ok and doc.srp and doc.srp.doc==e.doc and doc.srp.chunk==chunk and doc.srp.chunks==e.chunks and doc.srp.url==e.url and doc.srp.mode==e.mode then
   for _,im in ipairs(doc.images)do local v=e.images[im.asset];local bytes=v and read(v.file,65536);if bytes and #bytes==v.size and sum(bytes)==v.hash then im.file=root..v.file end end
   return doc
  end
 end
 function c:objects()
  if self.uncertain then return end
  local live=roots();if not live then return end
  local all={};local reset=true
  if smudge.browser_list_files then
   repeat local part,done=smudge.browser_list_files(root:gsub('/$',''),reset);reset=false;if not part then self.uncertain=true;return end;for _,n in ipairs(part)do all[#all+1]=n end;if done then break end;W.check()until #all>2048
   if smudge.browser_close_reader then smudge.browser_close_reader()end
  else all=smudge.list_files(root:gsub('/$',''))or {}end
  if #all>2048 then return end
  local total=0
  for _,n in ipairs(all)do if n:match('^b7%-%d+%.chunk$')or n:match('^b7%-%d+%.png$')then
   if not live[n]then smudge.delete_file(root..n)else total=total+live[n]end;W.check()
  end end
  for _,n in ipairs(all)do if n:match('^b7%-download%d+[%-%d]*%.png$')and not(smudge.browser_image_busy and smudge.browser_image_busy())then smudge.delete_file(root..n)end end
  return total
 end
 local function object(bytes,ext)
  if c.uncertain then return nil,'publication_uncertain'end
  local used=c:objects();if not used or used+#bytes>16*1024*1024 then return nil,'storage_limit'end
  local name
  for _=1,2048 do c.serial=c.serial%999999+1;name='b7-'..c.serial..ext;if not smudge.file_exists(root..name)then break end;name=nil end
  if not name then return nil,'storage_limit'end
  if not smudge.write_file(root..name,bytes)or read(name,#bytes)~=bytes then return nil,'storage_failed'end
  return name
 end
 local function candidate(e)
  local entries=J.array{e};for _,old in ipairs(c.entries)do if old.doc~=e.doc and #entries<8 then entries[#entries+1]=old end end;return entries
 end
 local function replace(e)return c:commit(candidate(e))end
 function c:save(doc,raw,defer)
  local t=doc.srp;local old=self:lookup(t.doc)
  if old and (old.url~=t.url or old.mode~=t.mode or old.chunks~=t.chunks)then return nil,'document_changed'end
  local wrapped=pack(1,raw);local name,err=object(wrapped,'.chunk');if not name then return nil,err end
  local e=old and J.decode(J.encode(old))or {doc=t.doc,url=t.url,title=t.title,mode=t.mode,chunks=t.chunks,files=J.object(),images=J.object()}
  e.files[tostring(t.chunk)]={file=name,size=#wrapped}
  local entries=candidate(e);if defer then return e,entries end
  local ok,why=self:commit(entries);if ok then return e end;return nil,why
 end
 function c:image(e,asset,bytes)
  if #bytes>65536 then return nil,'image_limit'end
  local file,err=object(bytes,'.png');if not file then return nil,err end
  local next=J.decode(J.encode(e));next.images[asset]={file=file,size=#bytes,hash=sum(bytes)}
  if replace(next)then return next,root..file end
 end
 return c
end
return B
