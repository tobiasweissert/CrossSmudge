-- Original synthetic cards; no publisher text or class identifiers.
local parts={};local path='test/browser/integration.lua'
for o=0,test_size(path)-1,8192 do parts[#parts+1]=test_read(path,o,math.min(8192,test_size(path)-o))end
local source=table.concat(parts);local prefix=assert(source:find('local article=',1,true))
local cases=[=[
local M,S,L=dofile('model.lua'),dofile('source.lua'),dofile('layout.lua')
local function plain(d)
 local a={};for _,b in ipairs(d.blocks)do local t={};for _,r in ipairs(b.spans)do t[#t+1]=r.text end;a[#a+1]=table.concat(t)end
 return table.concat(a,'\n')
end
-- Label/title boxes occur after a large CSS prelude, as on modern homepages.
local html='<head><style>'..string.rep('.unused{color:red}',1600)..'.label{display:flex}.headline{display:-webkit-box}</style></head><main><article><h2><span class="label"><span>Village AB</span></span><span class="headline" id="headline">«Quoted headline»</span></h2><p>co<strong>operate</strong>, mid<a href="/next">word</a>! <span class="unrelated">Un</span><span>broken</span>.</p></article></main>'
local function check(raw,reader)
 local d=assert(M.parse(raw,'https://example.org/',{reader=reader}));local p=plain(d)
 assert(p:find('Village AB\n«Quoted headline»',1,true),'CSS card boxes joined: '..p)
 assert(p:find('cooperate, midword! Unbroken.',1,true),'intentional inline words changed')
 local a=assert(L.find(d,'Quoted'));assert(d.blocks[a.b].spans[a.s].text:sub(a.p,a.p+5)=='Quoted','Find source-byte anchor changed')
 assert(d.anchors.headline and d.anchors.headline.b==a.b,'fragment moved off headline')
 local rows=L.page(d,nil,440,600);assert(#rows>0)
end
for _,reader in ipairs{true,false}do
 check(html,reader)
 check(S.new(#html,function(o,n)return html:sub(o+1,o+n)end),reader)
end
-- The closing CSS block boundary matters even if the following sibling is inline.
local tail=assert(M.parse('<style>.box{display:block}</style><p><span class="box">Label</span><span>Title</span></p>','https://example.org/'))
assert(plain(tail)=='Label\nTitle','closing styled box joined inline sibling')
-- Inline-block is still inline; unrelated spans and explicit UTF-8 spacing survive.
local nbsp='\194\160'
local inline=assert(M.parse('<style>.atom{display:inline-block}.line{display:inline}</style><p>co<span class="atom">operate</span><span class="line">!</span> A '..nbsp..'<b>B</b>.</p>','https://example.org/'))
assert(plain(inline)=='cooperate! A '..nbsp..'B.','inline display or explicit spacing changed')
-- Positive literal horizontal margins are rendered text gaps, not tag padding.
local meta='<style>'..string.rep('.unused{color:red}',1600)..'.stamp{margin:0 0.25rem}</style><article><p><span>Updated</span><time class="stamp" id="stamp">18 November</time><span>today</span></p><p>co<strong>operate</strong>! A<time>pril</time>.</p></article>'
for _,reader in ipairs{true,false}do
 for _,raw in ipairs{meta,S.new(#meta,function(o,n)return meta:sub(o+1,o+n)end)}do
  local d=assert(M.parse(raw,'https://example.org/',{reader=reader}));local p=plain(d)
  assert(p:find('Updated 18 November today',1,true),'horizontal CSS margins joined: '..p)
  assert(p:find('cooperate! April.',1,true),'unspaced time or inline word changed')
  local a=assert(L.find(d,'18 November'));assert(d.anchors.stamp.b==a.b and d.anchors.stamp.s==a.s and d.anchors.stamp.p==a.p,'margin corrupted Find/fragment byte anchor')
 end
end
-- Frozen boundary review: exact text, including absent whitespace, is the contract.
local failures={};local boundaryComparisons=0
local function boundaryCase(label,css,body,expected,external)
 local html='<style>'..css..'</style><p>'..body..'</p>'
 for _,reader in ipairs{true,false}do
  for _,raw in ipairs{html,S.new(#html,function(o,n)return html:sub(o+1,o+n)end)}do
   local d=assert(M.parse(raw,'https://example.org/',{reader=reader,css=external}));local actual=plain(d)
   boundaryComparisons=boundaryComparisons+1
   if actual~=expected then failures[#failures+1]=label..': '..string.format('%q',actual)..' ~= '..string.format('%q',expected)end
   local a=L.find(d,'operate')
   if a then assert(d.blocks[a.b].spans[a.s].text:sub(a.p,a.p+6)=='operate','boundary Find byte anchor')end
   if d.anchors.word then assert(a and d.anchors.word.b==a.b and d.anchors.word.s==a.s and d.anchors.word.p==a.p,'boundary fragment byte anchor')end
  end
 end
end
local word='co<span class="x" id="word">operate</span>!'
local far='/*'..string.rep('x',30000)..'*/'
boundaryCase('BIR-1 late zero','.x{margin-left:2px}'..far..'.x{margin-left:0}',word,'cooperate!')
boundaryCase('BIR-1 late important','.x{margin-left:2px}'..far..'.x{margin-left:0!important}',word,'cooperate!')
boundaryCase('BIR-1 early important','.x{margin-left:2px!important}'..far..'.x{margin-left:0}',word,'co operate!')
boundaryCase('BIR-1 specificity','.x{margin-left:2px}'..far..'span{margin-left:0}',word,'co operate!')
boundaryCase('BIR-1 prefix compound','p .x{margin-left:2px}'..far..'.x{margin-left:0}',word,'co operate!')
boundaryCase('BIR-1 descendant important','p .x{margin-left:2px}'..far..'.x{margin-left:0!important}',word,'cooperate!')
boundaryCase('BIR-1 equal specificity','.x{margin-left:2px}'..far..'.y{margin-left:0}','co<span class="x y">operate</span>!','cooperate!')
boundaryCase('BIR-1 wildcard compound order','*.x{margin-left:2px}'..far..'.x{margin-left:0}',word,'cooperate!')
boundaryCase('BIR-1 later prefix descendant','.x{margin-left:2px}/* kept source positions */ @media screen {.x{margin-left:3px}} *.x{margin-left:0}',word,'cooperate!')
boundaryCase('BIR-1 multiple sheets','.x{margin-left:2px}</style><style>'..far..'.x{margin-left:0}',word,'cooperate!')
boundaryCase('BIR-1 external before tail','</style><link rel="stylesheet" href="/style.css"><style>'..far..'.x{margin-left:0}',word,'cooperate!','.x{margin-left:2px}')
boundaryCase('BIR-1 external after inline','.x{margin-left:2px}</style><link rel="stylesheet" href="/style.css"><style>',word,'cooperate!','.x{margin-left:0}')
boundaryCase('BIR-1 important earlier sheet','.x{margin-left:2px!important}</style><style>'..far..'.x{margin-left:0}',word,'co operate!')
boundaryCase('BIR-1 display override','.x{display:flex}'..far..'.x{display:inline}',word,'cooperate!')
boundaryCase('BIR-1 display important','.x{display:flex}'..far..'.x{display:inline!important}',word,'cooperate!')
boundaryCase('BIR-1 inline normal',far..'.x{margin-left:0}', 'co<span class="x" style="margin-left:2px">operate</span>!','co operate!')
boundaryCase('BIR-1 inline vs important',far..'.x{margin-left:0!important}', 'co<span class="x" style="margin-left:2px">operate</span>!','cooperate!')
boundaryCase('BIR-1 inline important',far..'.x{margin-left:0!important}', 'co<span class="x" style="margin-left:2px!important">operate</span>!','co operate!')
for _,display in ipairs{'flex','grid','block','flow-root','list-item','table','-webkit-box'}do
 boundaryCase('BIR-2 hidden '..display,'.x{display:'..display..';margin:0 2px}','co<span style="display:none"><span class="x">omit</span></span><span id="word">operate</span>!','cooperate!')
 boundaryCase('BIR-2 hidden close '..display,'.x{display:'..display..'}','co<span hidden class="x">omit</span><span id="word">operate</span>!','cooperate!')
end
boundaryCase('BIR-2 suppressed parent','.x{display:flex}','co<template><span class="x">omit</span></template><span id="word">operate</span>!','cooperate!')
boundaryCase('BIR-2 visible whitespace','.x{display:grid}','co <span style="display:none"><span class="x">omit</span></span> operate!','co  operate!')
for _,selector in ipairs{'*|em','|em','em|*','*|*','[data-x]','span:hover','span > em','span + em','span ~ em','*.','*oops','.'}do
 boundaryCase('BIR-3 inert '..selector,selector..'{margin-left:2px}', 'co<span>operate</span>!','cooperate!')
end
for _,selector in ipairs{'*','span','span.x','.x','#word','p .x','p span.x#word','*.x'}do
 boundaryCase('BIR-3 supported '..selector,selector..'{margin-left:2px}',word,'co operate!')
end
assert(#failures==0,table.concat(failures,'\n'))
print('PASS boundary review exact text comparisons '..boundaryComparisons)
local C=dofile('css.lua');local index={}
local node={tag='span',attrs={class='x',id='word'}}
local parent={tag='p',attrs={},style={}}
local font=C.style(C.parse('p span.x#word{font-weight:bold;font-size:20px}'),{parent},node)
assert(font.bold and font.size==2,'valid presentation compound/descendant font style lost')
local inert=C.style(C.parse('*|em{font-weight:bold;font-size:20px}'),{parent},node)
assert(not inert.bold and not inert.size,'unsupported selector leaked presentation font style')
-- Static lengths only, shorthand/longhand order and priority, and hidden nodes.
for _,case in ipairs{
 {'margin:0 2px','co operate !'},
 {'margin:0 .2em 0 0','cooperate !'},
 {'margin:0 0 0 .25rem','co operate!'},
 {'margin-left:2px;margin:0','cooperate!'},
 {'margin:0 2px;margin-left:0;margin-right:0','cooperate!'},
 {'margin-left:2px!important;margin:0','co operate!'},
 {'margin:0 auto','cooperate!'}, {'margin:0 -2px','cooperate!'},
 {'margin:0 2%','cooperate!'}, {'margin:0 calc(2px)','cooperate!'},
 {'margin:0 var(--space)','cooperate!'}, {'margin:0 0','cooperate!'}
}do
 local raw='<p>co<span style="'..case[1]..'">operate</span>!</p>'
 assert(plain(assert(M.parse(raw,'https://example.org/')))==case[2],'margin literal/cascade contract '..case[1])
end
assert(plain(assert(M.parse('<p>co<span style="display:none;margin:0 2px">omit</span>operate!</p>','https://example.org/')))=='cooperate!','hidden margin leaked gap')
local rules='/* .fake{display:block} */ @media screen {.media{display:block}} .real{content:"}";display:flex}.tail{display:inline-flex}'
C.layout(index,{raw=rules,first=1,last=#rules})
assert(not index['.fake']and not index['.media']and index['.real'].value=='flex'and index['.tail'].value=='inline-flex','CSS scanner escaped comment/at-rule/string')
local bounded=string.rep('.a{display:block}',300)..'.late{display:flex}'
local idx={};C.layout(idx,{raw=bounded,first=1,last=#bounded});assert(idx.count==2,'duplicate selector exhausted index')
local huge='.a{unused:'..string.rep('x',10000)..';display:block}.b{display:flex}'
local boundedIndex={};C.layout(boundedIndex,{raw=huge,first=1,last=#huge});assert(not boundedIndex['.a']and boundedIndex['.b'],'oversized rule was not bounded/recovered')
print('PASS card spacing late CSS, Reader/Simplified, string/Source, inline words and anchors')
-- Exercise actual Direct callbacks, persistent Source and both reading modes.
local paint=smudge.text;local painted={}
smudge.text=function(x,y,t,...)
 if t=='Village AB'or t=='«Quoted headline»'then painted[t]={x=x,y=y}end
 return paint(x,y,t,...)
end
local function paintedGap()
 local a,b=assert(painted['Village AB']),assert(painted['«Quoted headline»'])
 assert(b.y>a.y,'production renderer joined CSS card boxes')
end
dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/cards');on_update();deliver(html);paintedGap()
assert(bodyText:find('Village AB',1,true)and bodyText:find('«Quoted headline»',1,true),'actual renderer omitted card')
click('...')
local switched=false
for _=1,8 do
 local found=false
 for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Simplified page mode'then found=true;break end end
 if found then painted={};click('Simplified page mode');paintedGap();switched=true;break end
 click('More')
end
assert(switched,'actual Simplified mode action missing')
painted={};on_exit();dofile('main.lua');on_init();draw();paintedGap()
assert(bodyText:find('Village AB',1,true)and bodyText:find('«Quoted headline»',1,true),'warm reopen omitted card')
print('PASS card spacing actual callbacks and saved Source reopen')
]=]
assert(load(source:sub(1,prefix-1)..cases,'card_spacing'))()
