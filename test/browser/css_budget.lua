smudge=browser_native
local load=dofile
function dofile(n)return load('apps/browser/'..n)end
local C,W=dofile('css.lua'),dofile('work.lua')
for _,case in ipairs({'comment','brace','colon','internal-space'})do
 local input
 if case=='comment'then input=string.rep('/*x',8000)
 elseif case=='brace'then input=string.rep('x',24000)
 elseif case=='colon'then input='p{'..string.rep('x',23900)..'}'
 else input='p{color:a'..string.rep(' ',23000)..'x}'end
 W.start(function()C.parse(input)end)
 local maximum=0;local steps=0
 while W.busy()do
  local start=os.clock();local ok,err=W.step();maximum=math.max(maximum,os.clock()-start);steps=steps+1;assert(ok~=nil,err)
 end
 print('CSS_NATIVE_QUANTUM',case,#input,maximum,steps)
 assert(maximum<0.030,'native CSS pattern blocks Lua hook: '..case..' '..maximum)
end

local Source,Model=dofile('source.lua'),dofile('model.lua')
local raw='<style>'..string.rep('/*x',8000)..'</style><article><p id="tail">Final content survives malformed CSS.</p></article>'
local source=Source.new(#raw,function(o,n)return raw:sub(o+1,o+n)end)
local result;W.start(function()result=assert(Model.parse(source,'https://example.org/'))end)
while W.busy()do local start=os.clock();local ok,err=W.step();assert(ok~=nil,err);assert(os.clock()-start<0.030)end
assert(result.blocks[1].spans[1].text=='Final content survives malformed CSS.'and result.anchors.tail.b==1)
local declarations=C.declarations('font-weight: bold !important;display:none; 42 bad  key : value ; color : a   !important x;')
assert(declarations['font-weight'][1]=='bold'and declarations['font-weight'][2]and declarations.display[1]=='none'and declarations.key[1]=='value'and declarations.color[1]=='a x')
print('PASS native CSS worst cases and actual Source/model callback quota preserve readable tail/visibility semantics')

local raw='<pre>a'..string.rep(' ',12000)..'x</pre>'
local source=Source.new(#raw,function(o,n)return raw:sub(o+1,o+n)end)
local doc;W.start(function()doc=assert(Model.parse(source,'https://example.org/'))end)
local maximum=0
while W.busy()do local start=os.clock();local ok,e=W.step();maximum=math.max(maximum,os.clock()-start);assert(ok~=nil,e)end
assert(maximum<0.030 and #doc.blocks==1,'native preformatted trim stalled')
local text={};for _,span in ipairs(doc.blocks[1].spans)do text[#text+1]=span.text end
assert(table.concat(text)=='a'..string.rep(' ',12000)..'x','preformatted spacing changed')
print('PASS preformatted native trim CPU budget/spacing',maximum)
