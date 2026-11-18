-- One bounded asynchronous GET at a time; all callbacks stay on the Lua loop.
local M,C,V=dofile('model.lua'),dofile('cache.lua'),dofile('presentation.lua')
local root='https://hacker-news.firebaseio.com/v0/';local feeds={'topstories','newstories','askstories','showstories'}
local S={screen='list',tab=1,listPage=1,ids={},items={},failed={},comments={},error='',loading=false,more=false}
local pending,active,closed,lastTry,feedCache,articleCache,deferred
local slots,frames,seen={},{},{};local target,bytes,visited=0,0,0
local function redraw()if V.retire then V.retire()end;smudge.request_update()end
local function cancel()
 if active and active.kind=='comment' then deferred=active end;if pending and pending.kind=='comment' then deferred=pending end
 if active then smudge.http_cancel(active.request) end;active=nil;pending=nil;S.loading=false;lastTry=nil
end
local function queue(work)if closed or active or pending then return false end;pending=work;S.loading=true;S.error='';redraw();return true end
local function url(id)return root..'item/'..id..'.json'end
local function fillList()
 for row=1,6 do local id=S.ids[(S.listPage-1)*6+row]
  if id and (not S.items[row] or S.items[row].id~=id) and not (S.failed[row] and S.failed[row].id==id) then queue{kind='item',id=id,row=row,url=url(id)};return end
 end
 local failed=0;for row=1,6 do if S.failed[row] then failed=failed+1 end end
 S.error=failed>0 and (failed..' stories unavailable; tap rows to retry') or '';S.loading=false;redraw()
end
local function failed(work,message)
 if work.kind=='item' then S.failed[work.row]={id=work.id,error=message};fillList()
 else S.error=message;if work.kind=='comment' then deferred=work end;redraw() end
end
local function retryRow(row)
 local id=S.ids[(S.listPage-1)*6+row];if not id then return end
 cancel();S.failed[row]=nil;queue{kind='item',id=id,row=row,url=url(id)}
end
local function itemSlots()
 slots={};for i=1,6 do slots[i]=C.open('item'..S.tab..i,M.item) end
end
local function cachedItems()
 S.items={};S.failed={}
 for row=1,6 do local item=slots[row] and slots[row].body and M.item(slots[row].body);if item and item.id==S.ids[(S.listPage-1)*6+row] then S.items[row]=item end end
end
local function loadPage(page)
 cancel();S.listPage=math.max(1,math.min(math.max(1,math.ceil(#S.ids/6)),page));S.error='';cachedItems()
 fillList();redraw()
end
local function nextComment()
 if deferred then local work=deferred;deferred=nil;return work.id,work.depth end
 while #frames>0 do local f=frames[#frames]
  if f.index>#f.ids then frames[#frames]=nil else local id=f.ids[f.index];f.index=f.index+1;if not seen[id] then return id,f.depth end end
 end
end
local function fillComments()
 -- Cursor and deferred work belong to the next admitted request.
 if closed or active or pending then return end
 if #S.comments>=target then S.loading=false;S.more=#frames>0 or deferred~=nil;redraw();return end
 if visited>=256 or bytes>=128*1024 then S.error='Thread session limit reached';S.more=false;S.loading=false;redraw();return end
 local id,depth=nextComment();if not id then S.loading=false;S.more=false;redraw();return end
 queue{kind='comment',id=id,depth=depth,url=url(id)}
end
local function openComments(row)
 local item=S.items[row];if not item then retryRow(row);return end
 cancel();deferred=nil;S.selected=item;S.selfText=M.plain(item.text);S.screen='comments';S.comments={};S.articleText=nil;S.error='';V.page=1
 frames={{ids=M.children(item.kids) or {},index=1,depth=0}};seen={};bytes=0;visited=0;target=6;S.more=true;fillComments();redraw()
end
local function article()
 if not S.selected then return end
 if S.screen~='article' then S.articleParent=S.screen=='comments' and 'comments' or 'list';S.articleParentPage=V.page end
 cancel();S.screen='article';V.page=1;S.error='';S.articleText=''
 if (not S.selected.url or S.selected.url=='') and S.selfText~='' then S.articleText=S.selfText;redraw();return end
 local id,raw=(articleCache.body or ''):match('^(%d+)\n(.*)$')
 if tonumber(id)==S.selected.id then S.articleText=M.plain(raw);redraw();return end
 local address=M.article_url(S.selected.url);if not address then S.error='This story has no supported article URL';redraw();return end
 S.articleText='';queue{kind='article',id=S.selected.id,url=address}
end
local function openArticle(row)
 local item=S.items[row];if not item then retryRow(row);return end
 cancel();deferred=nil;S.selected=item;S.selfText=M.plain(item.text);S.comments={};S.articleText=nil;S.screen='list';article()
end
function on_init()
 closed=false;V.init();feedCache=C.open('feed1',M.ids)
 itemSlots()
 articleCache=C.open('article',function(raw)local id,body=raw:match('^(%d+)\n(.*)$');return id and #body>0 and utf8.len(body)~=nil end,65556)
 S.ids=feedCache.body and M.ids(feedCache.body) or {};S.updated=feedCache.stamp and feedCache.stamp:match(' (%d%d:%d%d)')
 if #S.ids>0 then loadPage(1) else queue{kind='feed',url=root..feeds[1]..'.json'} end
end
function on_update()
 if closed or active or not pending then return end
 local now=smudge.millis();if lastTry and now-lastTry<200 then return end;lastTry=now
 if not smudge.has_feature('https_async') then local work=pending;pending=nil;S.loading=false;failed(work,'HTTPS unavailable on this host');return end
 local work=pending;local id,err=smudge.http_request{url=work.url,headers={Accept=work.kind=='article' and 'text/plain' or 'application/json'}}
 if id then pending=nil;work.request=id;active=work elseif err~='busy' then pending=nil;S.loading=false;failed(work,'Request not started: '..tostring(err)) end
end
function on_http(e)
 if closed or not active or e.id~=active.request then return end
 local work=active;active=nil;S.loading=false;lastTry=nil
 if e.error or e.status~=200 then
  local message=(e.error=='time_untrusted' or e.error=='time_sync_failed') and 'Time sync failed; check Wi-Fi and retry' or e.error=='body_limit' and 'Response exceeds this device limit' or ('Fetch failed: '..tostring(e.error or e.status))
  failed(work,message);return
 end
 local raw=e.body;if type(raw)~='string' or #raw>(work.kind=='article' and 65536 or 8192) then failed(work,'Invalid response size');return end
 if work.kind=='feed' then
  local ids=M.ids(raw);if not ids then S.error='Invalid story feed';redraw();return end
  S.ids=ids;S.listPage=1;cachedItems();feedCache:save(raw);S.updated=feedCache.stamp and feedCache.stamp:match(' (%d%d:%d%d)');fillList()
 elseif work.kind=='item' then
  local item=M.item(raw);if not item or item.id~=work.id then failed(work,'Story unavailable; tap to retry');return end
  S.failed[work.row]=nil;S.items[work.row]=item;slots[work.row]:save(raw);fillList()
 elseif work.kind=='comment' then
  local comment=M.comment(raw);visited=visited+1
  if not comment or comment.id~=work.id then S.error='Comment unavailable; go forward to retry';deferred=work;redraw();return end
  seen[work.id]=true
  if not comment.deleted then local text=M.plain(comment.text);bytes=bytes+#text;if bytes<=128*1024 then S.comments[#S.comments+1]={id=comment.id,by=comment.by,text=text,time=comment.time,depth=work.depth} end end
  local kids=M.children(comment.kids);if kids and #kids>0 and work.depth<4 then frames[#frames+1]={ids=kids,index=1,depth=work.depth+1} end;fillComments()
 elseif work.kind=='article' and S.selected and S.selected.id==work.id then
  if #raw==0 or not utf8.len(raw) then S.error='Invalid article text';redraw();return end
  S.articleText=M.plain(raw);if not articleCache:save(work.id..'\n'..raw) then S.error='Article loaded; cache write failed' end
 end;redraw()
end
local function turn(delta)
 if S.screen=='list' then loadPage(S.listPage+delta)
 elseif delta>0 and V.page==V.pages and S.screen=='comments' and S.more then target=math.min(256,#S.comments+6);fillComments()
 else V.page=math.max(1,math.min(V.pages,V.page+delta));redraw() end
end
local function back()
 if S.screen=='article' then
  cancel();S.screen=S.articleParent or 'list';V.page=S.articleParentPage or 1;S.error=''
  if S.screen=='list' then fillList() elseif S.more then fillComments() end
 elseif S.screen=='comments' then
  cancel();S.screen='list';V.page=1;S.error='';fillList()
 elseif active or pending then cancel();S.error='Cancelled'
 else on_exit();smudge.exit();return end
 redraw()
end
function on_button(button,pressed)
 if pressed==false or closed then return end
 if button=='back' then back() elseif button=='down' or button=='right' or button=='page_forward' then turn(1)
 elseif button=='up' or button=='left' or button=='page_back' then turn(-1)
 elseif button=='confirm' then if S.screen=='list' then if S.items[1] then openArticle(1) elseif #S.ids>0 then retryRow(1) else queue{kind='feed',url=root..feeds[S.tab]..'.json'} end elseif S.screen=='comments' then article() else back() end end
end
function on_touch(x,y,event)
 if closed or event~='tap' then return end;local kind,id=V.at(x,y)
 if kind=='tab' then cancel();deferred=nil;S.tab=id;feedCache=C.open('feed'..id,M.ids);S.ids=feedCache.body and M.ids(feedCache.body) or {};S.listPage=1;itemSlots();cachedItems();S.updated=feedCache.stamp and feedCache.stamp:match(' (%d%d:%d%d)');queue{kind='feed',url=root..feeds[id]..'.json'}
 elseif kind=='story' then openArticle(id) elseif kind=='comments' then openComments(id) elseif kind=='article' then article() elseif kind=='back' then back()
 elseif kind=='previous' then turn(-1) elseif kind=='next' then turn(1) elseif kind=='retry' then retryRow(id)
 elseif kind=='refresh' then queue{kind='feed',url=root..feeds[S.tab]..'.json'} end;redraw()
end
function on_swipe(dir)if not closed then turn((dir=='up' or dir=='left') and 1 or -1) end end
function on_draw()if not closed then if S.screen=='list' then V.list(S) else V.thread(S) end end end
function on_exit()closed=true;cancel();S.ids={};S.items={};S.failed={};S.comments={};S.articleText=nil;S.selfText=nil;S.selected=nil;deferred=nil;frames={};seen={};slots={};feedCache=nil;articleCache=nil end
return {state=function()return S end,view=V}
