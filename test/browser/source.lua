local host=dofile;function dofile(n)return host('apps/browser/'..n)end
local Source,M=dofile('source.lua'),dofile('model.lua')
local html='<html><head><style>p{font-weight:bold}</style></head><body><main><article><p>'..string.rep('café &amp; useful. ',2000)..'</p><div data-large="'..string.rep('x',11000)..'"></div><p>Final tail survives</p></article></main></body></html>'
local maxRead=0;local reads=0
local source=Source.new(#html,function(offset,count)assert(count<=8192);maxRead=math.max(maxRead,count);reads=reads+1;return html:sub(offset+1,offset+count)end)
local doc=assert(M.parse(source,'https://example.org/'));local text={};for _,b in ipairs(doc.blocks)do for _,r in ipairs(b.spans)do text[#text+1]=r.text end end
assert(table.concat(text):find('Final tail survives',1,true));assert(maxRead==8192 and reads>2)
print('PASS bounded source adapter + actual tokenizer, UTF8/entity chunk boundaries, oversized tag ancestry')
