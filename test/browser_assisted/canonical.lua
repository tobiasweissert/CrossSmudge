-- Frozen f6da7173 service fixture, exercised by the actual bounded production Lua.
local host=dofile
function dofile(p)return host('apps/browser/'..p)end
local J,P=dofile('json.lua'),dofile('protocol.lua')
local raw=assert(test_read('services/sticky-reader/testdata/ready.json',0,8192))
local doc=P.decode(raw,200)
assert(doc.blocks[2].spans[1].text=='Beginning café text with ')
assert(doc.blocks[2].spans[3].text==' and an ')
for _,id in ipairs({'picture','search'})do
 local a=assert(doc.anchors[id]);assert(a.s==1 and a.p==1)
 local b=doc.blocks[a.b];assert(b.image or b.form)
 local pos=P.anchor(doc,{block=b.id,run=1,byte=1});assert(pos.b==a.b)
 for _,p in ipairs({{block=b.id,run=2,byte=1},{block=b.id,run=1,byte=2}})do assert(not pcall(P.anchor,doc,p))end
end
local t=J.decode(raw);t.anchors[4].byte=2;assert(not pcall(P.ready,t))
t=J.decode(raw);t.anchors[4].run=2;assert(not pcall(P.ready,t))
t=J.decode(raw);t.anchors[2].byte=15;assert(not pcall(P.ready,t)) -- inside é
print('PASS f6da semantic whitespace, image/form anchors and forged-position controls')
