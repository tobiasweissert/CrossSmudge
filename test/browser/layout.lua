local load=dofile;function dofile(n)return load('apps/browser/'..n)end
smudge={text_width=function(s,font,style)return utf8.len(s)*(font=='lexend16'and 15 or 10)+(style=='bold'and 1 or 0)end,line_height=function()return 30 end}
local M,L=dofile('model.lua'),dofile('layout.lua')
local text='Start '..string.rep('word café supercalifragilisticexpialidocious ',100)..' end'
local doc=assert(M.parse('<article><p>'..text..'</p></article>','https://example.org/'))
local a={b=1,s=1,p=1};local all={};local n=0
repeat local rows,next,done=L.page(doc,a,280,220,0);assert(next.b>a.b or next.s>a.s or next.p>a.p);for _,row in ipairs(rows)do for _,run in ipairs(row.runs or {})do all[#all+1]=run.text end end;a=next;n=n+1;assert(n<200)until done
local function compact(s)return (s:gsub('%s',''))end
assert(compact(table.concat(all))==compact(text),'pagination lost or duplicated text')
local pre=assert(M.parse('<pre>    indent\n  next</pre>','https://example.org/'));local rows=L.page(pre,{},400,250,0);assert(rows[1].runs[1].text:sub(1,4)=='    ')
local U=dofile('url.lua');assert(U.resolve('https://example.org/a/b?q=1','')=='https://example.org/a/b?q=1');assert(U.resolve(nil,'https://example.org/a//b')=='https://example.org/a//b')
print('PASS UTF8 pagination progress/content preservation/pre/URL semantics')

-- The same URI references must agree with the native transport contract.
local fixtures=test_read('test/browser/fixtures/redirects.tsv',0,8192)
for reference,path in fixtures:gmatch('([^\t\n]+)\t([^\n]+)')do
 assert(U.wire(U.resolve('https://example.org/a/b?keep=1',reference))=='https://example.org'..path,reference)
end
