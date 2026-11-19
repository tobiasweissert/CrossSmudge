-- Existing Open Trivia DB provider/cache contract with the original card UI.
local M,C,U=dofile('model.lua'),dofile('cache.lua'),dofile('sticky_ui.lua')
local S={choice=1,status='Select to fetch a question',wait=0,waitStart=0,closed=false,answered=false}
local cache
local function show()U.retire();smudge.request_update()end
local function install(raw)
 S.q=M.question(raw);S.choice=1;S.answered=false;U.reset()
 if S.q then for i=4,2,-1 do local j=smudge.random(1,i);S.q.answers[i],S.q.answers[j]=S.q.answers[j],S.q.answers[i]end end
end
local function fetch()
 if S.request or S.queued then return end
 if S.wait>0 then S.status='Wait six seconds between fetches';show();return end
 S.queued=true;S.status='Queued';show()
end
local function choose(index)
 if not S.q or S.answered or type(index)~='number' or index%1~=0 or index<1 or index>4 then return end
 S.choice=index;S.answered=true;S.status=S.q.answers[index]==S.q.correct and 'Correct!' or 'Incorrect';U.reset();show()
end
local function act(kind,value)
 if kind=='fetch'then fetch()
 elseif kind=='choose'then choose(value)
 elseif kind=='detail'then U.detail=value;U.page=1;show()
 elseif kind=='question-detail'then U.detail=0;U.page=1;show()
 elseif kind=='choose-detail'then choose(U.detail)
 elseif kind=='close-detail'then U.reset();show()
 elseif kind=='page'then U.turn(value)
 elseif kind=='exit'then on_exit();smudge.exit()end
end
function on_init()
 S.closed=false;cache=C.open('question',M.question);U.init()
 if cache.body then install(cache.body);S.status='Offline cache: '..cache.stamp end
 show()
end
function on_update()
 if S.closed then return end
 if S.wait>0 then local elapsed=(smudge.millis()-S.waitStart)&0x7fffffff;if elapsed>=6000 then S.wait=0 end end
 if not S.queued or S.request then return end
 S.queued=false
 if not smudge.has_feature('https_async')then S.status='HTTPS unavailable on this host';show();return end
 local err;S.request,err=smudge.http_request{url='https://opentdb.com/api.php?amount=1&type=multiple&encode=url3986',headers={Accept='application/json'}}
 S.wait,S.waitStart=1,smudge.millis();S.status=S.request and 'Fetching question' or ('Not started: '..tostring(err));show()
end
function on_http(e)
 if S.closed or not S.request or e.id~=S.request then return end
 S.request=nil
 if e.error or e.status~=200 then S.status=e.error=='time_untrusted'and'Trust verified UTC in App Settings'or('Fetch failed: '..tostring(e.error or e.status))
 elseif not M.question(e.body)then S.status='Provider refused or invalid question. Retry later.'
 else install(e.body);S.status=cache:save(e.body)and'Live question saved'or'Live question; cache write failed'end
 show()
end
function on_draw()if not S.closed then U.draw(S);U.complete(S)end end
function on_button(b,pressed)
 if pressed==false or S.closed then return end
 if b=='back'then if U.detail then act('close-detail')else act('exit')end;return end
 if not U.valid(S)then return end
 if b=='page_back'or b=='page_forward'then
  if U.detail or not S.q then U.turn(b=='page_back'and -1 or 1);return end
  b=b=='page_back'and'up'or'down'
 end
 if b=='up'or b=='left'or b=='down'or b=='right'then
  if U.detail or not S.q then U.turn((b=='up'or b=='left')and -1 or 1)
  elseif not S.answered then S.choice=(S.choice-1+((b=='up'or b=='left')and -1 or 1))%5+1;U.keyboard=true;show()end
 elseif b=='confirm'then
  if U.detail then act((U.detail==0 or S.answered)and'close-detail'or'choose-detail')
  elseif not S.q or S.answered or S.choice==5 then fetch()
  elseif U.long and U.long[S.choice]then act('detail',S.choice)else choose(S.choice)end
 end
end
function on_touch(x,y,event)
 if S.closed or event~='tap'or not U.valid(S)then return end
 local w,h=smudge.get_bounds();if x<0 or y<0 or x>=w or y>=h then return end
 U.keyboard=false;local kind,value=U.touch(x,y);act(kind,value)
end
function on_swipe(direction)if not S.closed and U.valid(S)then U.turn((direction=='up'or direction=='left')and 1 or -1)end end
function on_exit()
 S.closed=true;S.queued=false;if S.request then smudge.http_cancel(S.request);S.request=nil end
 cache=nil;S.q=nil;U.reset()
end
return{state=function()return S end,view=U}
