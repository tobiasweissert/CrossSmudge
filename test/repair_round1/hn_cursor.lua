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


for _,phase in ipairs({'pending','active','retry-pending','retry-active'}) do
 for _,gesture in ipairs({'tap','swipe'}) do
  files={};requests={};cancels={};reader=dofile('main.lua');on_init()
  update();complete('[11]');update();complete(story(11,'[31,32,33]'));on_draw();on_touch(432,173,'tap')
  if phase:find('retry',1,true) then update();complete(nil,nil,503,'connect');on_swipe('up') end
  if phase:find('active',1,true) then update() end
  on_draw();for i=1,3 do if gesture=='tap' then on_touch(360,780,'tap') else on_swipe('up') end end
  update();assert(requests[#requests].url:find('/31.json',1,true),phase..' skipped 31')
  complete('{"id":31,"type":"comment","by":"one","text":"First reply"}')
  update();assert(requests[#requests].url:find('/32.json',1,true),phase..' '..gesture..' skipped 32')
  complete('{"id":32,"type":"comment","by":"two","text":"Second reply"}')
  update();assert(requests[#requests].url:find('/33.json',1,true))
  complete('{"id":33,"type":"comment","by":"three","text":"Third reply"}');assert(#reader.state().comments==3)
  local count=#requests;on_exit();complete('{"id":33,"type":"comment","text":"stale"}');update();assert(#requests==count and #reader.state().comments==0)
 end
end
print('PASS ordered 31/32/33, active/pending, footer/swipe, deferred retries, stale close')
