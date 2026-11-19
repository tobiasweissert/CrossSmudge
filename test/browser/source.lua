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

-- Count actual native substring requests, not wall-clock thresholds. Dense
-- resident plain searches must not materialize a block suffix per token.
local originalSub=string.sub;local suffixBytes=0
string.sub=function(value,first,last)
 if last==nil then local start=first<0 and math.max(1,#value+first+1)or math.max(1,first);suffixBytes=suffixBytes+math.max(0,#value-start+1)end
 return originalSub(value,first,last)
end
local ok,err=pcall(function()
 local input=Source.new(#dense,function(o,count)return originalSub(dense,o+1,o+count)end)
 local at,found=1,0
 while true do local a,b=input:find('<',at,true);if not a then break end;assert(a==b);found=found+1;at=b+1 end
 assert(found==4004)
end)
string.sub=originalSub;assert(ok,err)
assert(suffixBytes<#dense*2,'resident plain-find suffix copy bytes: '..suffixBytes)
for _,needle in ipairs({'<','<!--','-->','</script>','café','x.y','',string.rep('z',129)})do
 for shift=0,12 do
  local value=string.rep('x',8192-shift)..needle..'TAIL'..needle
  for _,first in ipairs({1,8190,#value,#value+1})do
   local input=Source.new(#value,function(o,count)return value:sub(o+1,o+count)end)
   local a,b=input:find(needle,first,true)
   -- Existing Source only searches while first <= size.
   local x,y;if first<=#value then x,y=value:find(needle,first,true)end
   assert(a==x and b==y,'plain find boundary mismatch')
  end
 end
end
print('PASS resident plain-find bounded copies + literal block-edge oracle',suffixBytes)

-- Reuse retained bounded tag bytes instead of probing the prior SD block
-- again for identity/self-close checks. Compare every emitted field to strings.
local Tokenizer=dofile('tokenizer.lua');local edgeReads=0
local function tokens(input)
 local out={};assert(Tokenizer.scan(input,function(kind,value,attrs,selfClose)
  local keys={};for k in pairs(attrs or {})do keys[#keys+1]=k end;table.sort(keys)
  local fields={kind,value or '',tostring(selfClose)}
  for _,k in ipairs(keys)do fields[#fields+1]=k..'='..attrs[k]end
  out[#out+1]=table.concat(fields,'\0')
 end));return table.concat(out,'\1')
end
for _,tag in ipairs({'<p title="quoted > café">','<br data-x="a" />','<!--edge-->','<![CDATA[café <edge>]]>','<script>x<y</ScRiPt>','<style>p{display:none}</StYlE>','<div title="'..string.rep('x',4300)..'">','<div title="'..string.rep('x',8500)..'" />'})do
 for shift=0,40 do
  local value=string.rep(' ',8192-shift)..tag..'<p>END café</p>'
  local input=Source.new(#value,function(o,count)edgeReads=edgeReads+1;return value:sub(o+1,o+count)end)
  assert(tokens(input)==tokens(value),'tokenizer block-edge oracle mismatch')
 end
end
-- Measured pre-change: 1580 reads. Require a real re-probe reduction;
-- no timing threshold or larger cache.
assert(edgeReads<1580,'bounded tag re-probe reads: '..edgeReads)
print('PASS bounded tag reuse + complete token edge oracle',edgeReads)
