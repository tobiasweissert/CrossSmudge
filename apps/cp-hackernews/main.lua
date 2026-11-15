-- Live public data only. No embedded feed or fixture fallback.
local M,C,U=dofile('model.lua'),dofile('cache.lua'),dofile('ui.lua')
local root='https://hacker-news.firebaseio.com/v0/'
local ids,item,index={},nil,1
local feed,story,article
local pending,active,closed
local screen,choice,status,articleText='menu',1,'Select Refresh to fetch',nil
local actions={'Read story','Read linked article (Jina)','Previous story','Next story','Refresh feed'}
local function redraw() smudge.request_update() end
local function queue(kind,url)
  if active or pending or closed then return end
  pending={kind=kind,url=url,id=ids[index]}; status='Queued'; redraw()
end
local function loadItem()
  item=nil; articleText=nil
  if story.body then
    local cached=M.item(story.body)
    if cached and cached.id==ids[index] then item=cached end
  end
  queue('item',root..'item/'..ids[index]..'.json')
end
function on_init()
  closed=false
  feed=C.open('feed',M.ids); story=C.open('story',M.item)
  article=C.open('article',function(s)
    local id,body=s:match('^(%d+)\n(.*)$')
    return id and #body>0 and utf8.len(body)~=nil
  end)
  ids=feed.body and M.ids(feed.body) or {}
  item=story.body and M.item(story.body) or nil
  if item then
    local found
    for i,id in ipairs(ids) do if id==item.id then index=i; found=true; break end end
    if not found then item=nil end
  end
  status=feed.body and ('Offline cache: '..feed.stamp) or 'No cache. Refresh feed.'
  redraw()
end
function on_update()
  if closed or active or not pending then return end
  local work=pending; pending=nil
  if not smudge.has_feature('https_async') then status='HTTPS unavailable on this host'; redraw(); return end
  local id,err=smudge.http_request{url=work.url,headers={Accept=work.kind=='article' and 'text/plain' or 'application/json'}}
  if id then work.request=id; active=work; status='Loading '..work.kind
  else status='Not started: '..tostring(err)..'. Select again.' end
  redraw()
end
function on_http(e)
  if closed or not active or e.id~=active.request then return end
  local work=active; active=nil
  if e.error or e.status~=200 then
    status=e.error=='time_untrusted' and 'Trust verified UTC in App Settings'
      or ('Failed: '..tostring(e.error or e.status)..'. Cache kept.')
    redraw(); return
  end
  local raw=e.body
  if type(raw)~='string' or #raw>8192 then status='Invalid response'; redraw(); return end
  local valid,saved
  if work.kind=='feed' then
    local nextIds=M.ids(raw)
    if nextIds then
      valid=true; saved=feed:save(raw); ids=nextIds; index=1; loadItem()
    end
  elseif work.kind=='item' then
    local nextItem=M.item(raw)
    if nextItem and nextItem.id==work.id and ids[index]==work.id then
      valid=true; saved=story:save(raw); item=nextItem
    end
  elseif work.kind=='article' and item and item.id==work.id and #raw>0 and utf8.len(raw) then
    valid=true
    articleText=M.plain(raw); screen='article'; U.page=1
    saved=article:save(item.id..'\n'..raw)
  end
  status=not valid and 'Invalid provider data. Cache kept.' or (saved and 'Fetched; saved for offline use' or 'Fetched; cache write failed')
  redraw()
end
function on_exit()
  closed=true; pending=nil
  if active then smudge.http_cancel(active.request); active=nil end
  ids={}; item=nil; articleText=nil; feed=nil; story=nil; article=nil
end
function on_button(b)
  if closed then return end
  if b=='back' then
    if active or pending then
      if active then smudge.http_cancel(active.request) end
      active=nil; pending=nil; status='Cancelled; cache kept'
    elseif screen~='menu' then screen='menu'; U.page=1
    else on_exit(); smudge.exit() end
    redraw(); return
  end
  local delta=(b=='up' or b=='left' or b=='page_back') and -1 or
    ((b=='down' or b=='right' or b=='page_forward') and 1 or nil)
  if delta then
    if screen=='menu' then choice=((choice-1+delta)%#actions)+1 else U.turn(delta) end
  elseif b=='confirm' then
    if screen~='menu' then screen='menu'; U.page=1
    elseif active or pending then return
    elseif choice==5 then queue('feed',root..'topstories.json')
    elseif choice==3 or choice==4 then
      if #ids>0 then index=math.max(1,math.min(#ids,index+(choice==3 and -1 or 1))); loadItem() end
    elseif choice==1 then
      if item then screen='story'; U.page=1
      elseif #ids>0 then loadItem() end
    elseif choice==2 and item then
      local cachedId,cachedBody=(article.body or ''):match('^(%d+)\n(.*)$')
      if tonumber(cachedId)==item.id then
        articleText=M.plain(cachedBody); screen='article'; U.page=1; status='Offline article: '..article.stamp
      else
        local url=M.article_url(item.url)
        if url then queue('article',url) else status='No supported public article URL' end
      end
    end
  end
  redraw()
end
function on_touch(x,y,event) if not closed then U.touch(x,y,event,on_button) end end
function on_draw()
  if closed then return end
  local body
  if screen=='menu' then
    local title=item and M.plain(item.title):gsub('%s+',' ') or 'No story loaded'
    local cut=utf8.offset(title,81)
    if cut then title=title:sub(1,cut-1)..'...' end
    body='Story '..index..' / '..#ids..'\n'..title..'\n\n'
      ..'Action '..choice..' / '..#actions..'\n> '..actions[choice]
      ..'\n\nPrev/Next choose; Select runs.\nJina receives selected article URLs.\n8 KiB limit; no browser/comments.'
  elseif screen=='article' then body=articleText or ''
  else body=M.plain(item.title)..'\nBy '..M.plain(item.by or '?')..'\n'..M.plain(item.url or '')..'\n\n'..M.plain(item.text or 'Link story. Use Read linked article in menu.') end
  U.draw('Hacker News',body,status,screen=='menu' and 'Select' or 'Menu')
end
