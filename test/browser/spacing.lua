-- Original synthetic content only; real production app callbacks and fonts.
local parts={};local path='test/browser/integration.lua'
for o=0,test_size(path)-1,8192 do parts[#parts+1]=test_read(path,o,math.min(8192,test_size(path)-o))end
local source=table.concat(parts);local prefix=assert(source:find('local article=',1,true))
local cases=[=[
local narrow='\226\128\175';local nbsp='\194\160';local join='\226\129\160'
local oracle=font_advance or function(t,f,style)
 local n=0;for _,c in utf8.codes(t)do n=n+((c==0x2060 or(c==0x202f and f~='ui12'))and 0 or c==32 and 7 or 10)end;return n
end
if not font_width then
 smudge.text_width=function(t,f,s)if t==''or t:match('^ +$')then return 0 end;return oracle(t,f,s)-2 end
end
local nativeWidth=smudge.text_width
smudge.browser_advance=nil
local M,L=dofile('model.lua'),dofile('layout.lua')
local function check(html)
 local d=assert(M.parse(html,'https://example.org/'));local rows=L.page(d,nil,440,400)
 for _,row in ipairs(rows)do for i=2,#row.runs do local a,b=row.runs[i-1],row.runs[i]
  assert(b.x>=a.x+oracle(a.text,a.font,a.bold and 'bold'or 'regular'),'fallback overlaps rendered predecessor: '..a.text..' x='..b.x)
 end end
 return d,rows
end
if os.getenv('BROWSER_SPACING_CASE')~='narrow'then
 check('<p>North <strong>south</strong> east</p>')
 check('<p>co<strong>operate</strong>, <a href="/next">éclair</a>! Fin.</p>')
 assert(L.advance,'shared body metric missing')
 -- Do not reuse primary sentinel metrics when an SD CJK fallback may resolve.
 for _,f in ipairs{'ui12','lexend14','lexend16'}do
  assert(L.advance('é中 mixed',f,'regular')==nativeWidth('é中 mixed',f,'regular'),'CJK font-context legacy path changed')
 end
 for _,f in ipairs{'ui12','lexend14','lexend16'}do for _,s in ipairs{'regular','bold'}do
  for c=32,126 do local t='é'..string.char(c)..'x ';assert(L.advance(t,f,s)==oracle(t,f,s),'sentinel boundary/kerning mismatch: '..t)end
  for _,t in ipairs{'',' ','North ','cooperate',',.!','éàü','a'..nbsp..'b','a'..join..'b','a\226\128\139b','  pre  text  '}do
   assert(L.advance(t,f,s)==oracle(t,f,s),'complete fallback advance mismatch: '..f..' '..s..' '..t)
  end
 end end
 print('PASS spacing fallback complete advances and styled boundaries')
end
if os.getenv('BROWSER_SPACING_CASE')~='fallback'then
 local raw='éNorth'..narrow..'south';local d={blocks={{spans={{text=raw,size=1}}}}}
 local rows,a=L.page(d,nil,440,200);local r=rows[1].runs[1]
 assert(rows[1].width==oracle(raw:gsub(narrow,' '),'lexend14','regular'),'Lexend narrow NBSP collapsed')
 assert(d.blocks[1].spans[1].text==raw,'source bytes changed')
 assert(L.find(d,'south').p==#('éNorth'..narrow)+1,'Find lost original byte anchor')
 assert(L.displayText(raw,'ui12')==raw,'supported narrow space changed')
 for _,f in ipairs{'lexend14','lexend16'}do
  assert(L.advance(raw,f,'regular')==oracle(raw:gsub(narrow,' '),f,'regular'),'fallback narrow advance mismatch')
  local called;smudge.browser_advance=function(t,font,style)called=t;return oracle(t,font,style)end
  assert(L.advance(raw,f,'bold')==oracle(raw:gsub(narrow,' '),f,'bold')and called==raw:gsub(narrow,' '),'native did not measure exact display text')
  smudge.browser_advance=nil
 end
 assert(L.displayText('a'..join..'b','lexend14')=='a'..join..'b','word joiner changed')
 -- Display substitution never creates a break opportunity: only forced long-token splitting.
 local one=L.page(d,nil,440,200);assert(#one==1)
 local pre={blocks={{spans={{text='  '..raw..'  ',size=1,pre=true}}}}};L.page(pre,nil,120,500)
 assert(pre.blocks[1].spans[1].text=='  '..raw..'  ')
 local token='North'..narrow..'south';local joined={blocks={{spans={{text='x '..token,size=1}}}}}
 local wrapped=L.page(joined,nil,oracle('North south','lexend14','regular')+1,300)
 assert(#wrapped==2 and wrapped[2].runs[1].text==token,'narrow NBSP became a line-break opportunity')
 local long=string.rep('éco'..narrow..'opérateur'..nbsp..'x'..join..'z',16)
 local longdoc={blocks={{spans={{text=long,size=1,pre=true}}}}};local anchor;local pieces={};local finished=false
 for i=1,200 do
  local page,nexta,done=L.page(longdoc,anchor,115,smudge.line_height('lexend14')+4)
  for _,row in ipairs(page)do for _,run in ipairs(row.runs)do pieces[#pieces+1]=run.text end end
  if done then finished=true;break end
  assert(not anchor or nexta.b~=anchor.b or nexta.s~=anchor.s or nexta.p~=anchor.p,'wrapped page made no progress')
  assert(nexta.p>#long or utf8.offset(long,0,nexta.p)==nexta.p,'long-page UTF8 anchor split')
  anchor=nexta
 end
 assert(finished and table.concat(pieces)==long,'wrapped/preformatted source bytes lost or duplicated')
 local _,nextAnchor=L.page(d,nil,80,smudge.line_height('lexend14')+4)
 assert(nextAnchor.p<=#raw+1 and(nextAnchor.p>#raw or utf8.offset(raw,0,nextAnchor.p)==nextAnchor.p),'split anchor inside UTF8')
 print('PASS spacing display-only narrow NBSP, nonbreaking token, pre and UTF8/Find anchors')
end
-- Actual main renderer must use the same display text and advance, including links.
smudge.browser_advance=oracle
local paints={};local paint=smudge.text
smudge.text=function(x,y,t,f,b,...)
 if t:find('North',1,true)then paints[#paints+1]={x=x,y=y,t=t,f=f,b=b}end
 return paint(x,y,t,f,b,...)
end
for _,native in ipairs{false,true}do
smudge.browser_advance=native and oracle or nil
paints={};files={};resources={};request=nil
dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/spacing');on_update()
deliver('<article><p>North <strong>south</strong> east co<strong>operate</strong>.</p><h2><a href="/gap">North'..narrow..'south</a></h2></article>')
local found=false;for _,p in ipairs(paints)do if p.f=='lexend14'and p.t:find('south',1,true)then assert(not p.t:find(narrow,1,true),'main drew unsupported separator');found=true end end
assert(found,'missing Lexend body paint')
w,h=800,480;draw();on_exit();dofile('main.lua');on_init();draw();assert(text:find('south',1,true),'warm reopen lost body')
local target
for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='North'..narrow..'south'then target=b end end
assert(target,'link label lost original bytes')
assert(target[3]==math.max(44,oracle('North south','lexend14','bold')),'link width differs from display advance')
on_tap(target[1]+target[3]-1,target[2]+2);draw();on_update();assert(request and request.q.url=='https://example.org/gap','visible link endpoint missed');deliver('<p>LINK_ENDPOINT</p>')
click('<');draw()
on_exit();w,h=480,800
end
print('PASS spacing actual callbacks both metric APIs, link endpoints, portrait/landscape and warm reopen')
]=]
assert(load(source:sub(1,prefix-1)..cases,'spacing'))()
