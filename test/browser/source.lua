local host=dofile;function dofile(n)return host('apps/browser/'..n)end
local Source,M=dofile('source.lua'),dofile('model.lua')
local html='<html><head><style>p{font-weight:bold}</style></head><body><main><article><p>'..string.rep('café &amp; useful. ',2000)..'</p><div data-large="'..string.rep('x',11000)..'"></div><p>Final tail survives</p></article></main></body></html>'
local maxRead=0;local reads=0
local source=Source.new(#html,function(offset,count)assert(count<=8192);maxRead=math.max(maxRead,count);reads=reads+1;return html:sub(offset+1,offset+count)end)
local doc=assert(M.parse(source,'https://example.org/'));local text={};for _,b in ipairs(doc.blocks)do for _,r in ipairs(b.spans)do text[#text+1]=r.text end end
assert(table.concat(text):find('Final tail survives',1,true));assert(maxRead==8192 and reads>2)
print('PASS bounded source adapter + actual tokenizer, UTF8/entity chunk boundaries, oversized tag ancestry')

-- Original synthetic heavy markup: hidden script and many nearby HTML tokens.
local heavy='<html><head><script>'..string.rep('var item="<not-html>";',20000)..'</script></head><body>'..'<p>'..string.rep('<b>x</b>',2400)..'</p>'..'<article><p>Heavy tail retained</p></article></body></html>'
local calls,bytes=0,0
local h=Source.new(#heavy,function(o,n)calls=calls+1;bytes=bytes+n;return heavy:sub(o+1,o+n)end)
assert(M.parse(h,'https://example.org/'))
assert(bytes<=#heavy*5,'source read amplification: '..calls..' reads / '..bytes..' bytes for '..#heavy)
for _,tag in ipairs({'<!-- comment -->','<script>ignored</ScRiPt>','<style>p{display:block}</StYlE>'})do
 for shift=0,35 do
  local raw=string.rep(' ',8192-shift)..tag..'<p>café &amp; final</p>'
  local parsed=assert(M.parse(Source.new(#raw,function(o,n)return raw:sub(o+1,o+n)end),'https://example.org/'))
  assert(parsed.blocks[#parsed.blocks].spans[1].text=='café & final')
 end
end
print('PASS heavy production source read budget',calls,bytes,#heavy)

local raw='<script>ignore</'..string.rep(' ',17000)..'ScRiPt'..string.rep(' ',17000)..'><p>Unbounded raw closing whitespace retained</p>'
local result=assert(M.parse(Source.new(#raw,function(o,n)return raw:sub(o+1,o+n)end),'https://example.org/'))
assert(result.blocks[1].spans[1].text=='Unbounded raw closing whitespace retained')
print('PASS stateful raw closing tag across whitespace longer than overlap')
local dense='<html><body>'..string.rep('<div><p>Hello</p></div>',1000)..'</body></html>'
local n,ioBytes,tokens=0,0,0
local denseSource=Source.new(#dense,function(o,count)n=n+1;ioBytes=ioBytes+count;return dense:sub(o+1,o+count)end)
assert(dofile('tokenizer.lua').scan(denseSource,function()tokens=tokens+1 end))
assert(tokens==5004 and ioBytes<#dense*8,'dense tokenizer read amplification')
print('PASS original dense 23KiB tokenizer',#dense,n,ioBytes,tokens)
