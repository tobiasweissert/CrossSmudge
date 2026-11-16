-- All literals below are SYNTHETIC adversarial fixtures, not captured responses.
local J = dofile('apps/cp-hackernews/json.lua')
local function bad(s) assert(J.decode(s)==nil, s) end
assert(J.decode('{"a":[1,true,null,"\\uD83D\\uDE00"]}').a[4]=='😀')
assert(J.decode('2147483647')==2147483647)
for _,s in ipairs({'01','1.','1e','[1,]','{"a":1,"a":2}','"\\uD800"','"\\uDC00"','"\255"','"\0"','2147483648',string.rep('[',14)..string.rep(']',14)}) do bad(s) end
print('PASS strict bounded JSON / int32 / UTF8')
local hostDofile=dofile
local app='apps/cp-hackernews/'
dofile=function(path) return hostDofile(app..path) end
local files,requests,cancels,draws={},{},{},{}
local nextId,inside,failWrite,exited=0,false,false,false
smudge={
  read_file=function(path,n)
    assert(not path:find('%.%.'));if path:match('^art/') then local f=assert(io.open(app..path,'rb'));local data=f:read(n);f:close();return data end
    return files[path] and files[path]:sub(1,n) end,
  write_file=function(path,body) files[path]=failWrite and body:sub(1,20) or body; return not failWrite end,
  get_date=function() return {year=2026,month=11,day=15,hour=12,min=0} end,
  has_touch=function() return true end,
  has_feature=function() return true end,
  http_request=function(opts) assert(not inside,'request during completion'); nextId=nextId+1; requests[#requests+1]=opts; return nextId end,
  http_cancel=function(id) cancels[#cancels+1]=id; return true end,
  request_update=function() end, exit=function() exited=true end,
  get_bounds=function() return 480,800 end,
  get_metrics=function() return {content_side_padding=20,header_height=40,top_padding=10,button_hints_height=40} end,
  text_width=function(s) assert(utf8.len(s)); return utf8.len(s)*8 end,
  line_height=function() return 20 end,
  clear=function() draws={} end,header=function() end,button_hints=function() end,
  text=function(x,y,s) assert(x>=0 and y>=50 and y<800); assert(utf8.len(s)*8<=440); draws[#draws+1]=s end
}
local C=dofile('cache.lua')
local c=C.open('test',function(s) return s=='old' or s=='new' end)
assert(c:save('old')); failWrite=true; assert(not c:save('new')); failWrite=false
assert(C.open('test',function() return true end).body=='old')
assert(c:save('new')); assert(C.open('test',function() return true end).body=='new')
local latest=c.name..c.slot..'.cache'
files[latest]=files[latest]:gsub('2026%-11','2027-11')
assert(C.open('test',function() return true end).body=='old')
print('PASS cache checked readback / interrupted writes / recovery')
local M=dofile('model.lua')
assert(M.ids('[11,22]')[2]==22)
assert(not M.ids('[11,11]')); assert(not M.ids('{"1":11}'))
assert(not M.item('{"id":1,"title":true,"type":"story"}'))
assert(not M.article_url('https://127.0.0.1/a'))
assert(not M.article_url('https://example.com@evil.test/a'))
assert(M.plain('<p>A &amp; &#x1F600;</p>')=='\nA & 😀')
local clock=0
smudge.millis=function()return clock end
smudge.time=function()return 100000 end
local bitmapCount=0
smudge.bitmap=function(x,y,w,h,data,white,gray)assert(gray==false,'pale text must not be dithered');assert(#data==((w+7)//8)*h);bitmapCount=bitmapCount+1 end
for _,name in ipairs({'rect','rect_dither','rounded_rect','line'}) do smudge[name]=function()end end
local reader=dofile('main.lua');on_init();on_draw();assert(bitmapCount>0)
local function update()clock=clock+250;on_update()end
local function complete(body,id,status,err)
 inside=true;on_http({id=id or nextId,status=status or 200,body=body,error=err});inside=false
end
local function story(id,kids)
 return '{"id":'..id..',"type":"story","title":"Synthetic story","by":"author","score":42,"time":99000,"descendants":2,"url":"https://example.com/","kids":'..(kids or '[]')..'}'
end


for _,kind in ipairs({'newlines','word','normal','utf8'}) do
 files={};requests={};reader=dofile('main.lua');on_init();update();complete('[11]');update();complete(story(11));on_draw();on_touch(150,160,'tap');update()
 local body=kind=='newlines' and string.rep('a\n',32768) or kind=='word' and string.rep('X',65536) or kind=='normal' and string.rep('Read this article. ',3449) or string.rep('Café 😀 ',5461)
 assert(#body<=65536);complete(body);on_draw();assert(reader.state().articleText==body and reader.view.pages>1)
 local pages=reader.view.pages;on_swipe('up');on_draw();assert(reader.view.page==2 and reader.view.pages==pages)
 while reader.view.page<pages do on_swipe('up') end;on_draw();assert(reader.view.page==pages and reader.state().error=='')
 on_exit();body=nil;collectgarbage('collect')
end
local ids={};for id=31,70 do ids[#ids+1]=id end
files={};requests={};reader=dofile('main.lua');on_init();update();complete('[11]');update();complete(story(11,'['..table.concat(ids,',')..']'));on_draw();on_touch(432,173,'tap')
for id=31,63 do
 update();assert(requests[#requests].url:find('/'..id..'.json',1,true))
 complete('{"id":'..id..',"type":"comment","by":"writer","text":"'..string.rep('a\\n',2000)..'"}')
 if (id-30)%6==0 then
  on_draw();while reader.view.page<reader.view.pages do on_swipe('up') end;on_swipe('up')
 end
end
assert(#reader.state().comments==32 and not reader.state().more)
assert(reader.state().error=='Thread session limit reached');on_draw();assert(reader.view.pages>1);on_exit()
-- Streaming and list wrapping retain identical UTF-8, whitespace and link text.
local U=dofile('design.lua');U.init()
for _,text in ipairs({'one\ntwo','Café 😀 link https://example.com/a',string.rep('X',200),'a  b\n\nc'}) do
 local old=U.wrap(text,140,'r20');local streamed={};U.lines(text,140,'r20',function(line)streamed[#streamed+1]=line end)
 assert(#old==#streamed);for i,line in ipairs(old) do assert(line==streamed[i]) end
end
print('PASS newline/wrap/normal/UTF8 64KiB articles and 128KiB comment limit; original wrap semantics')
