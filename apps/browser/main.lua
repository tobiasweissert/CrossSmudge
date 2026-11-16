-- Lua-first local e-ink browser. Direct HTTPS; no proxy, scripts or credentials.
local U,M,C,L,URL=dofile('ui.lua'),dofile('model.lua'),dofile('cache.lua'),dofile('layout.lua'),dofile('url.lua')
local S={status='Direct HTTPS • no scripts or logins',page=1,anchors={{b=1,s=1,p=1}},reader=true,font=0,history={},cursor=0,generation=0}
local Source=dofile('source.lua')
local owned={}
local function release(handle)if handle then smudge.http_release(handle);owned[handle]=nil end end
local function input(event)
 if not event.handle then return event.body end
 owned[event.handle]=true
 return Source.new(event.size,function(offset,count)return smudge.http_read(event.handle,offset,count)end)
end
local cache,active,pending,staged,closed,screen,menuPage,zoom,form,findText
local defaults={'https://feeds.bbci.co.uk/news/world/rss.xml','https://www.theguardian.com/world/rss','https://www.tagesschau.de/xml/rss2/'}
local function redraw()U.redraw()end
local function reset(anchor)S.page=1;S.anchors={anchor or {b=1,s=1,p=1}};S.next=nil;S.layoutKey=nil;U.focus=1 end
local function cancel()
 S.generation=S.generation+1;pending=nil;staged=nil;for handle in pairs(owned)do release(handle)end
 if active then smudge.http_cancel(active.id);active=nil end
end
local function remember()
 local entry=S.history[S.cursor]
 if entry then entry.anchor=S.anchors[S.page];entry.page=S.page;entry.anchors=S.anchors;entry.font=S.font;entry.reader=S.reader;entry.layoutKey=S.layoutKey end
end
-- Network and offline navigation retain the same bounded set of page anchors.
local function pushHistory(url,entry)
 for i=#S.history,S.cursor+1,-1 do S.history[i]=nil end
 if #S.history>=24 then table.remove(S.history,1)end
 S.history[#S.history+1]={url=url,entry=entry};S.cursor=#S.history
end
local function restore(entry,mode,view)
 local ok,doc,err=pcall(function()
  local raw=cache:source(entry);if not raw then return nil,'Cache corrupt or unavailable'end
  local reader=S.reader;if view and view.reader~=nil then reader=view.reader end
  return M.parse(raw,entry.url,{reader=reader,css=cache:css(entry)})
 end)
 if not ok or not doc then S.status='Offline parse failed: '..tostring(err or doc);return false end
 cancel();cache:clearImages()
 remember();if view then if view.reader~=nil then S.reader=view.reader end;S.font=view.font or S.font end
 S.doc,S.url,S.entry=doc,entry.url,entry
 if mode=='push'then pushHistory(entry.url,entry)end
 reset();S.status='Offline cache • '..doc.title;screen=nil;return true
end
local open
local function fragment(target)
 if not S.doc or URL.wire(target)~=URL.wire(S.url)then return false end
 local id=target:match('#(.+)$');if not id then return false end
 id=id:gsub('%%(%x%x)',function(c)return string.char(tonumber(c,16))end)
 local anchor=S.doc.anchors and S.doc.anchors[id]
 if anchor then remember();reset(anchor);S.url=target;return true end
 S.status='Fragment not found';return true
end
open=function(target,mode,index)
 target=URL.resolve(S.url,target)
 if not target then S.status='Use a public HTTPS address (max 512 bytes)';redraw();return end
 if fragment(target)then screen=nil;redraw();return end
 cancel();pending={url=URL.wire(target),target=target,kind='page',mode=mode or 'push',index=index,generation=S.generation,reader=S.reader,font=S.font}
 if mode=='history'and S.history[index]then local view=S.history[index];if view.reader~=nil then pending.reader=view.reader end;pending.font=view.font or S.font end
 S.status='Queued • Back cancels';screen=nil;zoom=nil;redraw()
end
local function address()
 U.edit('Address / search',S.url or '',512,function(text)
  if not text:find('%.')and not text:match('^https://')and text:find('%S')then text='https://en.wikipedia.org/w/index.php?search='..URL.encode(text)end
  local url=URL.resolve(nil,text);if url then open(url)else S.status='Enter a public HTTPS address'end
 end)
end
local function history(delta)
 local index=S.cursor+delta;local e=S.history[index];if not e then S.status='No '..(delta<0 and 'previous'or 'next')..' website';return end
 cancel();remember()
 if restore(e.entry,nil,e)then S.cursor=index;S.anchors=e.anchors or {e.anchor or {b=1,s=1,p=1}};S.page=e.page or 1;S.layoutKey=e.layoutKey;S.status='History • '..S.doc.title
 else open(e.url,'history',index)end
 screen=nil
end
local function turn(delta)
 if not S.doc then return end
 if delta>0 and S.next and not S.done then S.anchors[S.page+1]=S.next;S.page=S.page+1
 elseif delta<0 and S.page>1 then S.page=S.page-1 end
 redraw()
end
local function reparse()
 if not S.entry then return end
 local anchor=S.anchors[S.page]
 if restore(S.entry)then reset();S.status='Mode changed • start of document'end
end
local function finish(body,job,css,candidate,event)
 local ok,doc,err=true,candidate,nil
 if not doc then ok,doc,err=pcall(M.parse,body,job.url,{reader=job.reader,css=css})end
 if not ok or not doc then release(event and event.handle);S.status='Page kept • '..tostring(err or doc);redraw();return end
 -- Cache before swap. A partial SD write cannot replace the visible document.
 local savedOk,entry=pcall(cache.save,cache,job.url,body,css,event)
 if not savedOk or not entry then release(event and event.handle);S.status='SD save failed • previous page kept';redraw();return end
 release(event and event.handle)
 remember();cache:clearImages()
 if job.mode=='history'then S.cursor=job.index
 else pushHistory(job.url,entry)end
 local current=S.history[S.cursor];if current then current.entry=entry end
 S.reader=job.reader;S.font=job.font or S.font
 S.doc,S.url,S.entry=doc,job.url,entry;reset(job.mode=='history'and current and current.anchor or nil)
 if job.target and job.target:find('#',1,true)then fragment(job.url..'#'..job.target:match('#(.*)$'))end
 if job.mode=='history'and current then S.anchors=current.anchors or {current.anchor or {b=1,s=1,p=1}};S.page=current.page or 1;S.layoutKey=current.layoutKey end
 S.status=(doc.feed and 'Fetched feed • 'or 'Direct HTTPS • ')..(doc.truncated and 'text limit reached'or doc.cssLimited and 'CSS work limit • basic styles'or doc.title)
 cache:cleanup();redraw()
end
function on_init()
 closed=false;active=nil;pending=nil;staged=nil;screen=nil;zoom=nil;menuPage=1
 S={status='Direct HTTPS • no scripts/logins/paywall bypass',page=1,anchors={{b=1,s=1,p=1}},reader=true,font=0,history={},cursor=0,generation=0}
 if not smudge.has_feature('browser_v5')then S.status='Please update firmware: Browser needs API 5 + PSRAM';closed=true;redraw();return end
 cache=C.open();cache:cleanup();cache:clearImages();if #cache.sources==0 then cache.sources=defaults end
 if cache.entries[1]then restore(cache.entries[1],'push')end
 redraw()
end
function on_update()
 if closed or active or not pending then return end
 local job=pending;pending=nil
 if not smudge.has_feature('https_async')then S.status='Live HTTPS unavailable on this host; use offline cache';redraw();return end
 local id,err=smudge.http_request{url=job.url,timeout_ms=30000,resource=job.kind,headers={Accept=job.kind=='image'and 'image/png,image/jpeg'or job.kind=='css'and 'text/css'or 'text/html,application/rss+xml,application/atom+xml'}}
 if id then active=job;active.id=id;S.status='Loading '..job.kind..' • Back cancels'
 elseif err=='busy'then
  pending=job;if S.status~='Waiting for cancelled request'then S.status='Waiting for cancelled request';redraw()end;return
 else S.status='Request failed: '..tostring(err);staged=nil end
 redraw()
end
local function receive(e)
 if closed or not active or e.id~=active.id or active.generation~=S.generation then release(e.handle);return end
 local job=active;active=nil
 if e.error or e.status~=200 then
  if job.kind=='css'and staged then local stage=staged;staged=nil;finish(stage.body,stage.job,nil,nil,stage.event)
  else S.status=e.error=='time_untrusted'and 'Trust verified UTC in App Settings'or 'Fetch failed • '..tostring(e.error or e.status)..' • page kept';staged=nil end
  release(e.handle);redraw();return
 end
 job.url=URL.resolve(nil,e.url or job.url)or job.url
 if job.kind=='image'then
  local image=job.image
  local serial=job.generation;local name='b5-image'..serial..'.img';local path=smudge.browser_path(name)
  while smudge.file_exists(path)do serial=serial+1;name='b5-image'..serial..'.img';path=smudge.browser_path(name)end
  local magic=e.handle and smudge.http_read(e.handle,0,8)
  local supported=magic and (magic=='\137PNG\r\n\26\n'or magic:sub(1,3)=='\255\216\255')
  if e.handle and supported then
   if smudge.http_store(e.handle,name)then image.file=path;image.error=nil else image.error='SD image store failed'end
  else image.error='Only valid JPEG/PNG resources are supported'end
  release(e.handle);S.status=image.file and 'Image downloaded • tap to enlarge'or image.error
 elseif job.kind=='css'then
  local stage=staged;staged=nil
  if stage then
   local css=e.body
   if e.handle then local parts={};for offset=0,e.size-1,8192 do local count=math.min(8192,e.size-offset);local bytes,err=smudge.http_read(e.handle,offset,count);if type(bytes)~='string'or #bytes~=count then error(err or 'storage_read')end;parts[#parts+1]=bytes end;css=table.concat(parts)end
   release(e.handle);finish(stage.body,stage.job,css,nil,stage.event)
  else release(e.handle)end
 else
  local body=input(e)
  local ok,doc=pcall(M.parse,body,job.url,{reader=job.reader})
  if ok and doc and #doc.stylesheets>0 then
   staged={body=body,job=job,event=e.handle and e or nil};pending={url=doc.stylesheets[1],kind='css',generation=S.generation};S.status='Loading bounded stylesheet';doc=nil
  else finish(body,job,nil,ok and doc or nil,e.handle and e or nil)end
 end
 redraw()
end
function on_http(e)
 local ok,err=pcall(receive,e)
 if not ok then cancel();release(e.handle);S.status='Resource failed • page available: '..tostring(err);redraw()end
end
local function imageAction(image)
 if image.file then zoom=zoom==image and nil or image;screen=nil;return end
 cancel();pending={url=image.url,kind='image',image=image,slot=1,generation=S.generation};S.status='Image queued • direct HTTPS';redraw()
end
local function formAction(f)
 form=f;screen='form';menuPage=1
end
local function saveBookmark()
 if not S.url then return end
 for _,u in ipairs(cache.bookmarks)do if u==S.url then S.status='Already bookmarked';return end end
 if #cache.bookmarks>=20 then table.remove(cache.bookmarks,1)end
 cache.bookmarks[#cache.bookmarks+1]=S.url;S.status=cache:commit()and 'Bookmark saved'or 'Bookmark save failed'
end
local function find()
 U.edit('Find on page',findText or '',100,function(s)
  findText=s;local a=S.doc and L.find(S.doc,s,(S.anchors[S.page].b or 1)+1)
  if not a and S.doc then a=L.find(S.doc,s)end
  if a then reset(a);S.reflowTarget=a;screen=nil;S.status='Found: '..s else S.status='No match: '..s end
 end)
end
local function listActions()
 if screen=='news'then
  local rows={}
  for _,url in ipairs(cache.sources)do local target=url;rows[#rows+1]={target,function()open(target)end}end
  rows[#rows+1]={'Add feed URL',function()U.edit('HTTPS RSS / Atom feed','',512,function(s)local u=URL.resolve(nil,s);if u and #cache.sources<8 then cache.sources[#cache.sources+1]=u;cache:commit()end end)end}
  return rows
 elseif screen=='bookmarks'or screen=='offline'or screen=='history'then
  local rows={};local source=screen=='bookmarks'and cache.bookmarks or screen=='offline'and cache.entries or S.history
  for i,e in ipairs(source)do local item,index=e,i;local url=type(item)=='string'and item or item.url
   rows[#rows+1]={url,function()
    if screen=='offline'then if not restore(item,'push')then S.status='Cache corrupt or unavailable'end
    elseif screen=='history'then history(index-S.cursor)
    else open(url)end
   end}
  end
  return rows
 elseif screen=='form'then
  local rows={}
  for _,f in ipairs(form.fields)do if not f.hidden then local field=f;rows[#rows+1]={f.name..': '..f.value,function()U.edit(field.name,field.value,256,function(s)field.value=s end)end}end end
  rows[#rows+1]={'Submit GET search',function()local url=M.formURL(form);if url then open(url)else S.status='Search URL exceeds limits'end end};return rows
 end
 return {{'Address / search',address},{'Back website',function()history(-1)end},{'Forward website',function()history(1)end},
 {'Live news feeds',function()screen='news';menuPage=1 end},{'Bookmarks',function()screen='bookmarks';menuPage=1 end},
 {'Bookmark this page',saveBookmark},{'Offline pages',function()screen='offline';menuPage=1 end},{'History',function()screen='history';menuPage=1 end},
 {S.reader and 'Simplified page mode'or 'Reader mode',function()S.reader=not S.reader;reparse();screen=nil end},
 {'Text size: '..(S.font+1),function()local anchor=S.anchors[S.page];S.font=(S.font+1)%3;reset(anchor);S.reflowTarget=anchor;screen=nil end},
 {'Start of document',function()reset();screen=nil end},{'Find on page',find},{'Refresh page',function()if S.url then open(S.url)end end},
 {'Cancel loading',function()cancel();S.status='Cancelled • page kept';screen=nil end}}
end
function on_draw()
 if U.editor then U.editDraw();return end
 U.begin('Browser',U.fit(S.status,220,'small'))
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 U.button('<',p,t,44,44,function()history(-1)end)
 U.button(S.url and U.fit(S.url:gsub('^https://',''),w-2*p-120)or 'Address / search',p+50,t,w-2*p-106,44,address)
 U.button('...',w-p-50,t,50,44,function()screen=screen and nil or 'menu';menuPage=1 end)
 local y=t+56;local bottom=b-66
 if closed then U.wrap(S.status,p,y,w-2*p,4);return end
 if zoom then
  U.button('Close image',p,y,w-2*p,44,function()zoom=nil end)
  if not smudge.browser_image(zoom.file,p,y+52,w-2*p,math.max(1,bottom-y-52))then U.wrap('Image decoder rejected this image',p,y+60,w-2*p,3)end
 elseif screen then
  local actions=listActions();local n=math.max(1,(bottom-y-52)//50);local pages=math.max(1,math.ceil(#actions/n));menuPage=math.min(menuPage or 1,pages)
  for i=(menuPage-1)*n+1,math.min(menuPage*n,#actions)do local a=actions[i];U.button(U.fit(a[1],w-2*p-16),p,y+(i-(menuPage-1)*n-1)*50,w-2*p,44,a[2])end
  if pages>1 then U.button('More '..menuPage..'/'..pages,p,bottom-46,w-2*p,44,function()menuPage=menuPage%pages+1 end)end
  if #actions==0 then U.text(p,y,'Nothing saved yet')end
 elseif S.doc then
  local key=(w-2*p)..':'..(bottom-y)..':'..S.font
  if S.layoutKey and S.layoutKey~=key then S.reflowTarget=S.anchors[S.page]end
  if S.reflowTarget then
   local target=S.reflowTarget;local anchor={b=1,s=1,p=1};S.anchors={anchor};S.page=1
   local function after(a,b)return a.b>b.b or a.b==b.b and (a.s>b.s or a.s==b.s and a.p>b.p)end
   for page=1,512 do
    local _,next,done=L.page(S.doc,anchor,w-2*p,bottom-y,S.font)
    S.page=page
    if done or after(next,target)or (next.b==anchor.b and next.s==anchor.s and next.p==anchor.p)then break end
    S.anchors[page+1]=next;anchor=next
   end
   S.reflowTarget=nil
  end
  S.layoutKey=key
  local rows;rows,S.next,S.done=L.page(S.doc,S.anchors[S.page],w-2*p,bottom-y,S.font)
  for _,row in ipairs(rows)do
   local yy=y+row.y
   if row.image then
    local img=row.image;local drawn=img.file and smudge.browser_image(img.file,p,yy,w-2*p,row.height)
    if not drawn then smudge.rect(p,yy,w-2*p,row.height,false);U.wrap((img.error or img.alt)..' • tap to load JPEG/PNG',p+8,yy+8,w-2*p-16,2)end
    U.hit(p,yy,w-2*p,row.height,function()imageAction(img)end,'Image')
   elseif row.form then U.button('GET search form',p,yy,w-2*p,44,function()formAction(row.form)end)
   else
    for _,r in ipairs(row.runs)do
     local x=p+r.x;local rw=(smudge.browser_advance or smudge.text_width)(r.text,r.font,r.bold and 'bold'or 'regular')
     if r.block.style and r.block.style.background and r.block.style.background~='white'and r.block.style.background~='transparent'then smudge.rect_dither(x,yy,math.max(1,rw),row.height)end
     U.text(x,yy,r.text,r.font,r.bold)
     if r.href then
      local target=r.href;smudge.line(x,yy+row.height-2,x+rw,yy+row.height-2)
      U.hit(x,yy,math.min(w-p-x,math.max(44,rw)),44,function()open(target)end,r.text)
     end
     if r.block.style and r.block.style.border then smudge.line(p,yy+row.height,w-p,yy+row.height)end
    end
   end
  end
 else
  U.wrap('Direct public HTTPS browser. Reader mode, RSS/Atom news, bookmarks and checked offline pages. JavaScript, logins, paywalls and bot challenges are unsupported.',p,y,w-2*p,6)
  U.button('Live news feeds',p,bottom-100,w-2*p,44,function()screen='news';menuPage=1 end)
  U.button('Offline pages',p,bottom-50,w-2*p,44,function()screen='offline';menuPage=1 end)
 end
 local bw=(w-2*p-16)//3
 U.button('Previous',p,b-50,bw,44,function()turn(-1)end)
 U.button('Page '..S.page..(S.done and ' • end'or ''),p+bw+8,b-50,bw,44,function()screen='menu';menuPage=1 end)
 U.button('Next',p+2*(bw+8),b-50,bw,44,function()turn(1)end)
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if key=='back'then U.closeEditor()else U.key(key)end;redraw();return end
 if key=='back'then
  if active or pending then cancel();S.status='Cancelled • page kept'
  elseif zoom then zoom=nil elseif screen then screen=nil
  elseif S.cursor>1 then history(-1)else closed=true;smudge.exit()end
 elseif not screen and (key=='right'or key=='down'or key=='page_forward')then turn(1)
 elseif not screen and (key=='left'or key=='up'or key=='page_back')then turn(-1)
 elseif not screen and key=='confirm'then screen='menu';menuPage=1
 else U.key(key)end
 redraw()
end
function on_tap(x,y)U.tap(x,y)end
function on_swipe(dir)
 if U.editor or screen or zoom then return end
 if dir=='left'or dir=='up'then turn(1)elseif dir=='right'or dir=='down'then turn(-1)end
end
function on_exit()remember();closed=true;cancel();if cache then cache:clearImages()end end
