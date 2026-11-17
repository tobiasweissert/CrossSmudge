local host=dofile
function dofile(p)return host('apps/browser/'..p)end
local J=dofile('json.lua');local P=dofile('protocol.lua');local files={['saved/page1.cache']='B1:preserve',['saved/b5-index1']='B5:preserve'}
local failWrite,failRead=false,false
local objectReads=0
smudge={browser_path=function(n)return 'saved/'..n end,file_exists=function(p)return files[p]~=nil end,read_file=function(p,n)if p:match('%.chunk$')or p:match('%.png$')then objectReads=objectReads+1 end;if failRead and p:find('b7-index',1,true)then return end;return files[p]and files[p]:sub(1,n)end,write_file=function(p,b)local fail=failWrite and p:find('b7-index',1,true);files[p]=fail and b:sub(1,4)or b;return not fail end,delete_file=function(p)files[p]=nil end,list_files=function()local a={};for p in pairs(files)do a[#a+1]=p:sub(7)end;return a end}
local W={check=function()end,atomic=function(f)f()end};local B=dofile('b7cache.lua');local c=B.open(W)
local function chunk(id,n)
 local t={v=1,state='ready',doc=string.rep(id,32),url='https://example.org/',title='Original synthetic',lang='en',dir='ltr',mode='reader',chunks=2,chunk=n or 0,truncated=false,blocks=J.array{{id=1,kind='p',runs=J.array{{text='Original content'}}}},anchors=J.array()};local raw=J.encode(t);return P.decode(raw,200),raw
end
local doc,raw=chunk('a');local e=assert(c:save(doc,raw));assert(c:load(e,0));assert(not c:load(e,1));assert(c:coverage(e)==1);objectReads=0;assert(c:objects());assert(objectReads==0,'GC/quota must use checked lengths without whole-document reads')
local old=files['saved/b7-index2'];failWrite=true;doc,raw=chunk('b');assert(not c:save(doc,raw));failWrite=false;assert(c.uncertain);assert(files['saved/b7-index2']==old);local count=0;for _ in pairs(files)do count=count+1 end;assert(not c:save(doc,raw));local after=0;for _ in pairs(files)do after=after+1 end;assert(count==after)
-- A malformed root latches read-only, including GC; healthy reread does not invent safety.
files['saved/b7-index1']='B7:broken';local uncertain=B.open(W);assert(uncertain.uncertain);local snapshot={};for k,v in pairs(files)do snapshot[k]=v end;assert(not uncertain:objects());assert(not uncertain:save(doc,raw));for k,v in pairs(snapshot)do assert(files[k]==v)end
files['saved/b7-index1']=nil;local healthy=B.open(W);assert(healthy:load(healthy.entries[1],0),'healthy load');failRead=true;local unreadable=B.open(W);assert(unreadable.uncertain,'unreadable latch');failRead=false;assert(not unreadable:objects(),'objects latch')
-- Both roots keep immutable objects; startup never deletes persistent PNG references.
local e=healthy.entries[1];local imageEntry,path=healthy:image(e,string.rep('f',32),'synthetic validated image');assert(imageEntry);local png=files[path];local reopened=B.open(W);assert(reopened:objects());assert(files[path]==png)
assert(files['saved/page1.cache']=='B1:preserve'and files['saved/b5-index1']=='B5:preserve')
print('PASS B7 checked publication/read failure/uncertain roots/both-slot references/persistent images; B5/B1 unchanged')
