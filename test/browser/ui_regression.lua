local host=dofile
function dofile(n)return host('apps/browser/'..n)end
local calls=0;local w,h=800,480
smudge={get_bounds=function()return w,h end,text_width=function(s)calls=calls+1;return utf8.len(s)*9 end,line_height=function()return 26 end}
local U=dofile('ui.lua')
for _,s in ipairs({string.rep('x',512),string.rep('é',256)})do
 calls=0;local fitted=U.fit(s,300);assert(calls<=12,'linear fitting: '..calls);assert(utf8.len(fitted));assert(fitted:sub(-3)=='...','missing ellipsis');assert(smudge.text_width(fitted)<=300)
end
print('PASS bounded UTF8 address fit')

for _,size in ipairs({{800,480},{480,800}})do w,h=size[1],size[2];local r=U.content(0);assert(r.x==14 and r.y==78+60+26 and r.width==w-28 and r.height==h-smudge.line_height('small')-16-r.y);assert(r.key==r.width..':'..r.height..':0')end
print('PASS shared content geometry')

local painted={}
smudge.get_metrics=function()return{}end
smudge.clear=function()painted={}end
smudge.rect=function()end;smudge.button_hints=function()end
smudge.request_update=function()end
smudge.text=function(x,y,s,font)if font=='ui12' and s:find('|',1,true)then painted[#painted+1]=s;assert(smudge.text_width(s)<=w-28,'draft overflow')end end
smudge.wrapped_text=function(s)return{lines={s:sub(1,20)..'...'}}end
local function click(label)
 U.editDraw();for _,b in ipairs(U.buttons)do if b[6]==label then U.activate(b);return end end;error(label)
end
for _,size in ipairs({{800,480},{480,800}})do
 w,h=size[1],size[2]
 for _,draft in ipairs({string.rep('x',512),string.rep('é',256),'head\n'..string.rep('abc',160)..'tail'})do
 U.edit('Address',draft,512,function()end)
 local function visible()U.editDraw();assert(#painted==1,'caret hidden');assert(U.editor.value==draft,'text changed')end
 visible()
 for i=1,utf8.len(draft)do click('<');visible()end;assert(U.editor.cursor==1)
 click('q');assert(U.editor.value==draft or U.editor.value=='q'..draft)
 if #draft<512 then click('Delete');assert(U.editor.value==draft)end
 for i=1,utf8.len(draft)do click('>');visible()end;assert(U.editor.cursor==#draft+1)
 click('Delete');assert(U.editor.value==draft:sub(1,(utf8.offset(draft,-1)or 1)-1));U.closeEditor()
 end
end
print('PASS full UTF8 draft actual button traversal end-start-end and delete')

local L=dofile('layout.lua')
local doc={blocks={{spans={{text=string.rep('word ',50)}}}}}
local rows=L.page(doc,{},180,180,0)
assert(#rows>=5,'prose inherits touch minimum')
for _,r in ipairs(rows)do assert(r.height==30)end
local links=L.page({blocks={{spans={{text='A',href='/a'},{text='B',href='/b'}}}}},{},180,180,0)
assert(links[1].height>=44 and links[1].runs[2].x>=44,'links overlap')
print('PASS prose density and independent link targets')
