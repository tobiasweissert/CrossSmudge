local host=dofile
function dofile(p)return host('apps/browser/'..p)end
local J,P=dofile('json.lua'),dofile('protocol.lua')
local function maxchunk(n)
 local t={v=1,state='ready',doc=string.rep('a',32),url='https://example.org/synthetic',title=string.rep('T',200),lang='en',dir='auto',mode='reader',chunks=64,chunk=n,truncated=false,blocks=J.array(),anchors=J.array()}
 for b=1,64 do
  local runs=J.array();for r=1,4 do runs[r]={text=string.rep(string.char(96+r),29),href='https://example.org/link'..b,size=r%3,bold=r%2==0}end
  t.blocks[b]={id=b,kind='p',runs=runs};t.anchors[b]={id='anchor'..b,block=b,run=1,byte=1}
 end
 return J.encode(t)
end
local raw=maxchunk(0);assert(#raw<=49152);local a=P.decode(raw,200);local b=P.decode(maxchunk(1),200)
assert(#a.blocks==64 and #b.blocks==64)
local history={};for i=1,24 do local files={};for n=0,63 do files[tostring(n)]={file='b7-'..(i*64+n)..'.chunk',size=49180}end;history[i]={entry={doc=string.rep('a',32),url='https://example.org/'..i,files=files,chunks=64},anchor={b=1,s=1,p=1},page=1}end
collectgarbage('collect');assert(collectgarbage('count')<3500)
for _=1,64 do local v=P.decode(raw,200);assert(#v.blocks==64)end
local function bad(mutate)local t=J.decode(raw);mutate(t);assert(not pcall(P.decode,J.encode(t),200))end
bad(function(t)t.blocks[2].id=1 end)
bad(function(t)t.blocks[1].runs[1].text=string.rep('x',2048)end)
bad(function(t)t.anchors[1].run=5 end)
bad(function(t)t.anchors[1].byte=31 end)
bad(function(t)t.blocks[1].runs[1].bold=0 end)
print('PASS maximum 64 blocks/256 runs/64 hrefs/64 anchors; two chunks + history24; repeated decode at 4MiB')
