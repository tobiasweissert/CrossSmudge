-- Optional real captured input validation. Provenance must accompany bodies.
local root=assert(os.getenv('CP_FIXTURES'),'set CP_FIXTURES to capture directory')
local host=dofile
local function read(name)
  local f=assert(io.open(root..'/'..name..'.body','rb'));local raw=f:read('*a');f:close();return raw
end
dofile=function(p) return host('apps/cp-hackernews/'..p) end
local H=dofile('model.lua')
local ids=assert(H.ids(read('feed')))
local item=assert(H.item(read('item')));assert(item.id==ids[1])
local article=read('article');assert(#article<=8192 and utf8.len(article))
dofile=function(p) return host('apps/cp-trivia/'..p) end
assert(dofile('model.lua').question(read('trivia')))
-- This archive is deliberately outside APIv3; measure it without allocating it.
local archive=assert(io.open(root..'/connections.body','rb'))
local connections_bytes=assert(archive:seek('end'));archive:close()
assert(connections_bytes>8192,'expected the documented archive-size blocker')
print('Captured Connections archive bytes: '..connections_bytes..'; v3 max 8192')
print('PASS captured provider shapes; not device TLS/framing proof')
