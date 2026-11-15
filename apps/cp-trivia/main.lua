-- One real Open Trivia DB question per explicit fetch; no fabricated fallback.
local M,C,U=dofile('model.lua'),dofile('cache.lua'),dofile('ui.lua')
local cache,q,request,queued,closed,answered
local choice,status,wait=1,'Select to fetch a question',0
local waitStart=0
local function show() U.page=1; smudge.request_update() end
local function install(raw)
  q=M.question(raw); choice=1; answered=false
  if q then
    -- Shuffle without storing credentials or a provider session token.
    for i=4,2,-1 do local j=smudge.random(1,i); q.answers[i],q.answers[j]=q.answers[j],q.answers[i] end
  end
end
function on_init()
  closed=false
  cache=C.open('question',M.question)
  if cache.body then install(cache.body); status='Offline cache: '..cache.stamp end
  show()
end
function on_update()
  if closed then return end
  -- millis wraps on-device; use unsigned modular arithmetic via Lua integers.
  if wait>0 then
    local now=smudge.millis()
    local elapsed=(now-waitStart)&0x7fffffff
    if elapsed>=6000 then wait=0 end
  end
  if not queued or request then return end
  queued=false
  if not smudge.has_feature('https_async') then status='HTTPS unavailable on this host'; show(); return end
  local err
  request,err=smudge.http_request{url='https://opentdb.com/api.php?amount=1&type=multiple&encode=url3986',headers={Accept='application/json'}}
  wait,waitStart=1,smudge.millis()
  status=request and 'Fetching question' or ('Not started: '..tostring(err))
  show()
end
function on_http(e)
  if closed or not request or e.id~=request then return end
  request=nil
  if e.error or e.status~=200 then
    status=e.error=='time_untrusted' and 'Trust verified UTC in App Settings' or 'Fetch failed: '..tostring(e.error or e.status)
  elseif not M.question(e.body) then status='Provider refused or invalid question. Retry later.'
  else
    install(e.body)
    status=cache:save(e.body) and 'Live question saved' or 'Live question; cache write failed'
  end
  show()
end
function on_exit()
  closed=true; queued=false
  if request then smudge.http_cancel(request); request=nil end
  cache=nil; q=nil
end
function on_button(b)
  if closed then return end
  if b=='back' then on_exit(); smudge.exit(); return end
  if b=='up' or b=='left' or b=='page_back' then U.turn(-1)
  elseif b=='down' or b=='right' or b=='page_forward' then U.turn(1)
  elseif b=='confirm' then
    if request or queued then return end
    if not q or answered or choice==5 then
      if wait>0 then status='Wait six seconds between fetches'; smudge.request_update(); return end
      queued=true; status='Queued'
    else
      answered=true
      status=q.answers[choice]==q.correct and 'Correct!' or 'Incorrect'
    end
    show()
  end
end
-- Answer selection has dedicated visible touch rows only on the menu page;
-- button users cycle with a separate select/answer screen below.
local mode='choose'
local handle=on_button
function on_button(b)
  if closed then return end
  if q and not answered and mode=='choose' then
    if b=='up' or b=='left' or b=='page_back' then choice=(choice-2)%5+1; show(); return end
    if b=='down' or b=='right' or b=='page_forward' then choice=choice%5+1; show(); return end
    if b=='confirm' and choice~=5 then mode='read'; show(); return end
  end
  if b=='back' and mode=='read' then mode='choose'; show(); return end
  handle(b)
  if answered then mode='choose' end
end
function on_touch(x,y,event) if not closed then U.touch(x,y,event,on_button) end end
function on_draw()
  if closed then return end
  local body='Open Trivia DB\nCC BY-SA 4.0\nSelect: fetch one live question.\nNo bundled questions.'
  local hint='Fetch'
  if q then
    if answered then
      body=q.question..'\n\nCorrect answer:\n'..q.correct..'\n\nSelect: next live question.'
      hint='Next'
    elseif mode=='read' then
      body=q.question..'\n\nYour answer:\n'..q.answers[choice]..'\n\nSelect to submit. Back to choose.'
      hint='Answer'
    else
      local label=choice==5 and 'Skip / fetch next question' or q.answers[choice]
      local cut=utf8.offset(label,81); if cut then label=label:sub(1,cut-1)..'...' end
      body='Choice '..choice..' / 5\n'..label..'\n\nPrev/Next choose an answer.\nSelect: read question and full answer.\nChoice 5 skips this question.\n\nOpen Trivia DB / CC BY-SA 4.0'
      hint='Read'
    end
  end
  U.draw('Trivia',body,status,hint)
end
