local host=dofile
function dofile(p)return host('apps/browser/'..p)end
local J=dofile('json.lua');local P=dofile('protocol.lua')
local function bad(s)assert(not pcall(J.decode,s),s)end
for _,s in ipairs({'{"a":1,"a":2}','{"x":"\\u0000"}','{"x":"\\ud800"}','{"x":"\\udc00"}','{"x":"\255"}','{} trailing','[1.0]','[1000000]','[01]','[[[[[[[[[[[[[0]]]]]]]]]]]]]','{"x":"\\r"}'})do bad(s)end
assert(J.decode('{"x":"\\ud83d\\ude00","n":null}').x=='\240\159\152\128')
assert(J.decode(J.encode({a='a\n"\\',b=true})).a=='a\n"\\')
local ready={v=1,state='ready',doc=string.rep('a',32),url='https://example.org/',title='Synthetic',lang='en',dir='ltr',mode='reader',chunks=2,chunk=0,truncated=false,blocks=J.array{{id=1,kind='p',runs=J.array{{text='Tail link',href='https://example.org/two'}}}},anchors=J.array{{id='top',block=1,run=1,byte=1}}}
assert(P.decode(J.encode(ready),200).blocks[1].spans[1].text=='Tail link')
for _,mutate in ipairs({function(t)t.unknown=true end,function(t)t.chunks=65 end,function(t)t.blocks[1].runs[1].text=string.rep('a',2049)end,function(t)t.anchors[1].byte=100 end,function(t)t.doc='bad'end,function(t)t.blocks[1].runs[1].href='https://127.0.0.1/'end,function(t)t.blocks=J.object{}end})do local t=J.decode(J.encode(ready));mutate(t);assert(not pcall(P.decode,J.encode(t),200))end
local continued=J.decode(J.encode(ready));continued.blocks[1].continued=false;assert(P.decode(J.encode(continued),200));continued.blocks[1].continued=0;assert(not pcall(P.decode,J.encode(continued),200))
local canonical=J.decode(J.encode(ready));canonical.url='https://EXAMPLE.org';assert(P.decode(J.encode(canonical),200).srp.url=='https://example.org/');canonical.url='https://example.org/ ';assert(not pcall(P.decode,J.encode(canonical),200))
print('PASS strict JSON and SRP1 malformed controls')
