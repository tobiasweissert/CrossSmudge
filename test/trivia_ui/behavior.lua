-- Actual Lua input callbacks, original masks, production libraries/identity/raster.
assert(smudge.get_device()=='sticky','actual configured identity did not select Sticky')
assert(utf8.len('Café')==4 and io==nil and package==nil and debug==nil,'production network library set differs')
local assetRead,artifactWrite=smudge.read_file,smudge.write_file
local files,requests,now,cancelled,failWrite,exited={}, {}, 0, nil, false, false
smudge.read_file=function(path,n,offset)
 if path:match('^art/') then return assetRead(path,n,offset)end
 local s=files[path];return s and s:sub((offset or 0)+1,(offset or 0)+n)
end
smudge.write_file=function(path,s)if failWrite then return false end;files[path]=s;return true end
smudge.get_date=function()return {year=2026,month=11,day=16,hour=12,min=0}end
smudge.millis=function()return now end
smudge.random=function(a,b)return b end
smudge.has_feature=function(f)return f=='https_async'end
smudge.http_request=function(options)requests[#requests+1]=options;return #requests end
smudge.http_cancel=function(id)cancelled=id end
smudge.exit=function()exited=true end
local traceFile
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do
 local native=smudge[name]
 smudge[name]=function(...)
  if name=='bitmap' then
   local x,y,w,h=...;local vw,vh=smudge.get_bounds()
   assert(x>=0 and y>=0 and x+w<=vw and y+h<=vh,'actual glyph bitmap escaped viewport')
  end
  if traceFile then
   local out={name};for i,v in ipairs({...})do out[#out+1]=name=='bitmap'and i==5 and v:gsub('.',function(c)return string.format('%02x',c:byte())end)or tostring(v)end
   assert(artifactWrite(traceFile,table.concat(out,'\t')..'\n',true))
  end
  return native(...)
 end
end
local app=dofile('main.lua')
local function state()return app.state()end
local function draw()on_draw()end
local function shot(name)
 traceFile=name..'.trace';artifactWrite(traceFile,'',false);draw();traceFile=nil;capture(name..'.pgm')
 artifactWrite(name..'.state.txt',state().status..'\nchoice='..state().choice..'\nanswered='..tostring(state().answered)..'\n'..(state().q and state().q.question or 'no question'),false)
end
local function tap(kind,value)
 draw()
 for _,hit in ipairs(app.view.hit)do if hit.kind==kind and (value==nil or hit.value==value)then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');draw();return end end
 error('Missing actual drawn touch target '..kind..':'..tostring(value))
end
local function button(b)on_button(b,true);draw()end
local function raw(question,correct,other)
 return '{"response_code":0,"results":[{"type":"multiple","question":"'..question..'","correct_answer":"'..correct..'","incorrect_answers":["'..other[1]..'","'..other[2]..'","'..other[3]..'"]}]}'
end
local function complete(body,status,error)
 on_http{id=state().request,status=status or 200,body=body,error=error};draw()
end
local function request()
 now=now+6000;on_update();tap('fetch');assert(state().queued);on_update();assert(state().request)
end
on_init();shot('notice')
assert(type(app)=='table'and app.view,'graphical Trivia did not install')
tap('fetch');assert(#requests==0,'request during input callback');on_update();assert(#requests==1)
assert(requests[1].url=='https://opentdb.com/api.php?amount=1&type=multiple&encode=url3986')
on_http{id=999,status=200,body='bad'};assert(state().request==1,'stale callback consumed request')
-- Keep actual provider payload ordering; random identity makes correct slot1.
local fixture=raw('Which%20planet%20is%20known%20as%20the%20Red%20Planet%3F','Mars',{'Mercury','Venus','Jupiter'})
complete(fixture);shot('choice')
button('page_forward');assert(state().choice==2,'physical PageForward cannot select next answer');button('page_back');assert(state().choice==1,'physical PageBack cannot select previous answer')
local before=state().choice
on_touch(-1,450,'tap');on_touch(10000,450,'tap');on_touch(50,400,'move');assert(not state().answered and state().choice==before)
tap('choose',2);assert(state().answered and state().status=='Incorrect' and state().q.answers[2]=='Mercury','touch selected wrong option or failed to answer');shot('wrong')
tap('fetch');on_update();assert(#requests==1,'cooldown bypassed')
request();complete(fixture);button('confirm');assert(state().answered and state().status=='Correct!','physical confirm did not score selected option');shot('correct')
-- Cached replay keeps provider data, shuffle, local stamp, and answer semantics.
on_exit();app=dofile('main.lua');on_init();draw();assert(state().q.question=='Which planet is known as the Red Planet?'and state().status:find('Offline cache'))
button('right');button('confirm');assert(state().answered and state().choice==2 and state().status=='Incorrect')
-- Request errors preserve the previous checked cache and question.
request();complete(nil,0,'time_untrusted');assert(state().status=='Trust verified UTC in App Settings'and state().q)
request();complete('{"response_code":5,"results":[]}');assert(state().status:find('Provider refused'))
-- Failed writes accept the live question but preserve the earlier cache.
request();failWrite=true;complete(raw('New%20question%3F','New',{'Old','Other','Else'}));failWrite=false
assert(state().status=='Live question; cache write failed')
on_exit();app=dofile('main.lua');on_init();draw();assert(state().q.question=='Which planet is known as the Red Planet?','failed write displaced earlier cache')
-- Long unbroken words, mixed case, Latin-1 and unsupported Unicode are never
-- silently dropped: exact original masks or explicit U+ escape text.
local longq=string.rep('LongMixedCaseQuestion',65)..'%20Caf%C3%A9%20%CF%80%3F'
local longa=string.rep('VeryLongAnswerWord',70)..'%20Caf%C3%A9%20%F0%9F%98%80'
request();complete(raw(longq,longa,{'Short','Another','Last'}));assert(not state().answered)
assert(app.view.questionLong,'long question not marked for complete reading')
local readDoor=false;local _,height=smudge.get_bounds()
for _,hit in ipairs(app.view.hit)do if hit.kind=='question-detail'and hit.box.y==height-80 then readDoor=true end end
assert(readDoor,'long question has no visible READ footer control')
tap('question-detail');assert(app.view.detail==0)
local function allPages()
 local out={};assert(app.view.page==1);draw() -- real detail callback starts on page one
 local total=app.view.pages;assert(total<1000)
 for page=1,total do
  assert(app.view.page==page);for _,line in ipairs(app.view.visibleLines)do out[#out+1]=line end
  button('page_forward')
 end
 return table.concat(out):gsub('%s','')
end
local G=dofile('graphics.lua');G.init()
local visible=allPages();assert(visible==G.visible(state().q.question,'body'):gsub('%s',''),'question tail unreachable')
shot('long-question-last-page');button('back');assert(app.view.detail==nil and not state().answered)
tap('detail',1);assert(app.view.detail==1)
visible=allPages();assert(visible==G.visible(state().q.question..'\n\n'..state().q.answers[1],'body'):gsub('%s',''),'long answer tail unreachable')
shot('long-answer-last-page');tap('choose-detail');assert(state().answered and state().status=='Correct!')
-- Signed millis wrap preserves the exact six-second boundary.
now=2147483640;on_update();tap('fetch');on_update();assert(state().request);complete(fixture)
local count=#requests;now=-2147477657;on_update();tap('fetch');on_update();assert(#requests==count,'rollover cooldown released early')
now=-2147477656;on_update();tap('fetch');on_update();assert(#requests==count+1)
local outstanding=state().request;on_exit();assert(cancelled==outstanding)
on_http{id=outstanding,status=200,body=fixture};on_update();on_touch(240,750,'tap');assert(not state().q and #requests==count+1,'closed app handled late work')
app=dofile('main.lua');on_init();draw();tap('exit');assert(exited,'END control did not exit')
print('PASS actual touch/button choices, source banner/glyphs, cache, cooldown/rollover, errors, long question/answers, close/reload')
