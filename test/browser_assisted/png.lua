local host=dofile
function dofile(p)return host('apps/browser/'..p)end
local PNG=dofile('png.lua');local path=assert(os.getenv('ASSISTED_PNG_FIXTURE'));local size=test_size(path)
assert(PNG.validate(function(p,n)return test_read(path,p,n)end,size,128,64))
assert(not PNG.validate(function(p,n)return test_read(path,p,n)end,size,127,64))
local bytes=test_read(path,0,size)
for _,at in ipairs({1,17,25,29,36,#bytes-1})do local b=bytes:sub(1,at-1)..string.char(bytes:byte(at)~1)..bytes:sub(at+1);assert(not PNG.validate(function(p,n)return b:sub(p+1,p+n)end,#b,128,64))end
assert(not PNG.validate(function(p,n)return (bytes..'x'):sub(p+1,p+n)end,#bytes+1,128,64))
print('PASS true palette1 PNG envelope/CRC/bounds/no trailing data')
