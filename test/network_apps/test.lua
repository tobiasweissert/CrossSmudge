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
update();assert(#requests==1);complete('[11,22]');assert(#requests==1)
update();complete(story(99));assert(reader.state().failed[1])
on_draw();on_touch(150,160,'tap');update();complete(story(11,'[31]'))
update();complete(story(22));assert(reader.state().items[2].id==22)
on_draw();on_touch(432,173,'tap');assert(reader.state().screen=='comments')
update();assert(requests[#requests].url:find('/31.json',1,true))
complete('{"id":31,"type":"comment","by":"one","text":"First reply","kids":[32]}')
update();complete('{"id":32,"type":"comment","by":"two","text":"Nested reply"}')
assert(#reader.state().comments==2 and reader.state().comments[2].depth==1)
on_draw();local articleButton
for _,b in ipairs(reader.view.hit) do if b.kind=='article' then articleButton=b end end
assert(articleButton);on_touch(articleButton.x+5,articleButton.y+5,'tap');update()
assert(requests[#requests].url=='https://r.jina.ai/https://example.com/')
local article=string.rep('Read this long article. ',2700);complete(article);on_draw()
assert(reader.state().articleText==article and reader.view.pages>1)
local page=reader.view.page;on_swipe('up');on_draw();assert(reader.view.page==page+1)
on_button('back');assert(reader.state().screen=='comments');on_button('back');assert(reader.state().screen=='list')
-- Changing tabs cancels its request; a late callback cannot replace state.
on_draw();on_touch(180,85,'tap');update();local cancelled=nextId
on_touch(300,85,'tap');assert(cancels[#cancels]==cancelled)
complete('[99]',cancelled);assert(#reader.state().ids==0)
update();complete('[44]');update();complete(story(44));assert(reader.state().items[1].id==44)
on_exit();local count=#requests;update();on_draw();assert(#requests==count)
-- Top and Ask have separate journals; reopening uses Top.
reader=dofile('main.lua');on_init();assert(reader.state().ids[1]==11 and reader.state().items[1].id==11,'switching feeds lost offline Top stories')
on_draw();on_touch(432,173,'tap');update();complete(nil,nil,0,'time_untrusted')
assert(reader.state().error:find('check Wi-Fi and retry',1,true));on_exit()
assert(not M.item('{"id":1,"type":"story","title":"Bad","score":"boom"}'))
assert(not M.comment('{"id":1,"type":"comment","text":"Hi","by":"a","time":{}}'))
local U=dofile('design.lua');U.init()
for _,line in ipairs(U.wrap(string.rep('X',200),140,'r20')) do assert(U.width(line,'r20')<=140) end
assert(#U.wrap('one\ntwo',400,'r20')==2)
print('PASS HN identity / nested replies / article paging / cancellation / offline cache / long words')

-- One failed middle row must not prevent later metadata requests. Actual app
-- callbacks, original bitmap masks and fake asynchronous transport only.
files={};requests={};cancels={};reader=dofile('main.lua');on_init();update();complete('[11,22,33,44,55,66]')
update();complete(story(11,'[71]'));update();complete(nil,nil,503,'connect')
assert(reader.state().items[1] and reader.state().failed[2])
for _,id in ipairs({33,44,55,66}) do update();assert(requests[#requests].url:find('/'..id..'.json',1,true));complete(story(id)) end
assert(reader.state().items[6].id==66 and not reader.state().loading)
assert(reader.state().error:find('1 stories unavailable',1,true))
local function tap(kind,id)
 on_draw()
 for _,b in ipairs(reader.view.hit) do if b.kind==kind and (id==nil or b.id==id) then
  local x=kind=='story' and b.x+100 or b.x+b.w//2;on_touch(x,b.y+b.h//2,'tap');return
 end end
 error('Missing '..kind..':'..tostring(id))
end
-- Retry is for the failed row, not the next unrelated story.
tap('retry',2);update();assert(requests[#requests].url:find('/22.json',1,true));complete(story(22));assert(reader.state().items[2].id==22 and not reader.state().failed[2])
-- A title goes straight to an article. Loading Back returns on the first press.
tap('story',1);assert(reader.state().screen=='article');update();local cancelledArticle=nextId
assert(requests[#requests].url=='https://r.jina.ai/https://example.com/')
on_button('back');assert(reader.state().screen=='list' and cancels[#cancels]==cancelledArticle)
complete('Late article',cancelledArticle);assert(reader.state().screen=='list' and reader.state().articleText=='')
tap('story',1);update();complete('Cached direct article');assert(reader.state().articleText=='Cached direct article');on_button('back')
local before=#requests;tap('story',1);update();assert(#requests==before and reader.state().articleText=='Cached direct article');on_button('back')
-- Badge opens comments; Read retains that parent and resumes cancelled replies.
tap('comments',1);assert(reader.state().screen=='comments');update();local reply=nextId
tap('article');assert(reader.state().screen=='article');assert(cancels[#cancels]==reply);on_button('back');assert(reader.state().screen=='comments')
update();assert(requests[#requests].url:find('/71.json',1,true));complete('{"id":71,"type":"comment","by":"reader","text":"Reply"}')
assert(#reader.state().comments==1);on_button('back');assert(reader.state().screen=='list')
-- Self posts show their existing body without making an article request.
reader.state().items[3].url=nil;reader.state().items[3].text='<p>Self &amp; post</p>'
before=#requests;tap('story',3);update();assert(#requests==before and reader.state().articleText=='\nSelf & post');on_button('back');assert(reader.state().screen=='list')
-- Oversized article responses stay on Article with an honest error and Back.
tap('story',4);on_draw();assert(reader.state().loading);update();complete(nil,nil,0,'body_limit');on_draw();assert(reader.state().screen=='article' and reader.state().error=='Response exceeds this device limit' and reader.state().articleText=='');on_button('back');assert(reader.state().screen=='list')
-- Opening an already loaded story while later rows load cancels that request;
-- returning to the list resumes it, instead of leaving placeholders forever.
on_exit();files={};requests={};reader=dofile('main.lua');on_init();update();complete('[11,22,33]');update();complete(story(11));update();local listRequest=nextId
assert(requests[#requests].url:find('/22.json',1,true));tap('story',1);assert(cancels[#cancels]==listRequest)
on_button('back');assert(reader.state().screen=='list');update();assert(requests[#requests].url:find('/22.json',1,true));complete(story(22));update();complete(story(33));assert(reader.state().items[3].id==33)
-- Body/identity failures also advance, and a request-start failure advances.
on_exit();files={};requests={};reader=dofile('main.lua');on_init();update();complete('[11,22,33,44]');update();complete(story(11));update();complete(string.rep('x',8193));assert(reader.state().failed[2]);update();complete(story(999));assert(reader.state().failed[3]);update();complete(story(44));assert(reader.state().items[4].id==44)
local realRequest=smudge.http_request;smudge.http_request=function()return nil,'worker_failed'end
tap('retry',2);update();assert(reader.state().failed[2]);smudge.http_request=realRequest
-- Busy retains the pending request and retries after the normal interval.
local busy=true;smudge.http_request=function(opts)if busy then busy=false;return nil,'busy' end;return realRequest(opts) end
tap('retry',3);before=#requests;update();assert(#requests==before and reader.state().loading);update();assert(#requests==before+1);complete(story(33));assert(reader.state().items[3].id==33)
smudge.http_request=realRequest;on_exit()
print('PASS HN row-failure continuation/retry, direct/cache/selfpost article, badge comments, parent Back and interrupted-list resume')
