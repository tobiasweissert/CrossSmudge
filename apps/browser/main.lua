-- Lua-first local e-ink browser. Direct HTTPS; no proxy, scripts or credentials.
browser_launch_api=1 -- Native optional launch handshake; older installed packages fail closed.
local U,M,C,L,URL=dofile('ui.lua'),dofile('model.lua'),dofile('cache.lua'),dofile('layout.lua'),dofile('url.lua')
local S={status='Direct HTTPS • no scripts or logins',page=1,anchors={{b=1,s=1,p=1}},reader=true,font=0,history={},cursor=0,generation=0}
local Source=dofile('source.lua')
local W=dofile('work.lua')
local owned={}
local function hasResources()return next(owned)~=nil end
local layoutRequest,imageRequest,preparingImage
local imageSerial=0
local decodedFiles={}
local function release(handle)if handle then smudge.http_release(handle);owned[handle]=nil end end
local function input(event)
 if not event.handle then return event.body end
 owned[event.handle]=true
 return Source.new(event.size,function(offset,count)return smudge.http_read(event.handle,offset,count)end)
end
local completion -- One owned event waits behind ordinary UI work; HTTP has one active slot.
local cache,active,pending,staged,closed,screen,menuPage,zoom,form,findText
local startupComplete,launchChecked=false,false
local defaults={'https://feeds.bbci.co.uk/news/world/rss.xml','https://www.theguardian.com/world/rss','https://www.tagesschau.de/xml/rss2/'}
local function redraw()U.redraw()end
local function reset(anchor)S.page=1;S.anchors={anchor or {b=1,s=1,p=1}};S.next=nil;S.layoutKey=nil;S.prepared=nil;U.focus=1 end
-- One bounded view lease shares the old model/rows; it copies no response or
-- framebuffer. It survives yields until the new visible page is ready.
local viewKeys={'doc','url','entry','page','anchors','next','done','font','reader','layoutKey','prepared','reflowTarget','history','cursor'}
local function retainView()
 if S.rollbackView or not S.doc or not S.prepared then return end
 local previous={};for _,key in ipairs(viewKeys)do previous[key]=S[key]end;S.rollbackView=previous
end
local function restoreView()
 local previous=S.rollbackView;S.rollbackView=nil
 if previous then for _,key in ipairs(viewKeys)do S[key]=previous[key]end end
 S.reflowTarget=nil;layoutRequest=nil
 -- Cancel a whole-book rotation walk, then prepare just the retained position
 -- in the current orientation. Do not silently restart the cancelled walk.
 if S.prepared then local w,h=smudge.get_bounds();if S.prepared.sw~=w or S.prepared.sh~=h then S.layoutKey=nil end end
end
local function firstPage(doc,anchor,page,font)
 local w,h=smudge.get_bounds();local m=smudge.get_metrics();local pad=math.max(12,m.content_side_padding or 20)
 local width=w-2*pad
 local top=m.top_padding+m.header_height+12+56;local bottom=h-m.button_hints_height-12-66
 local height=bottom-top
 local key=width..':'..height..':'..font
 local rows,next,done=L.page(doc,anchor,width,height,font)
 return {doc=doc,page=page,key=key,rows=rows,sw=w,sh=h},next,done
end
local function installPage(prepared,next,done)
 S.prepared,S.next,S.done,S.layoutKey,S.reflowTarget=prepared,next,done,prepared.key,nil
 local w,h=smudge.get_bounds();if prepared.sw==w and prepared.sh==h then S.rollbackView=nil end
end
local function cancel()
 if active then smudge.http_cancel(active.id);active=nil end
 if not W.running()then W.cancel();restoreView()end
 layoutRequest=nil
 if imageRequest then imageRequest.image.error='Image cancelled • tap to retry'end
 if preparingImage then preparingImage.error='Image cancelled • tap to retry';preparingImage=nil;zoom=nil end
 imageRequest=nil
 if smudge.browser_image_cancel then smudge.browser_image_cancel()end
 if smudge.browser_close_reader then smudge.browser_close_reader()end
 S.generation=S.generation%999999+1;pending=nil;staged=nil;completion=nil;for handle in pairs(owned)do release(handle)end
end
local function awaitDrain()
 while true do
  local image=smudge.browser_image_busy and smudge.browser_image_busy()
  local http=smudge.http_busy and smudge.http_busy()
  if not image and not http then return end
  W.stage(image and 'Waiting for cancelled image'or 'Waiting for cancelled request');W.check()
 end
end
local function queue(fn,stage)
 if W.busy()then cancel()end
 W.start(fn,stage);redraw()
end
U.action=function(fn,value)queue(function()fn(value)end,'Preparing')end
local function remember()
 local view=S.rollbackView or S;local entry=view.history[view.cursor]
 if entry then entry.anchor=view.anchors[view.page];entry.page=view.page;entry.anchors=view.anchors;entry.font=view.font;entry.reader=view.reader;entry.layoutKey=view.layoutKey end
end
-- Network and offline navigation retain the same bounded set of page anchors.
local function historyCandidate(url,entry)
 local candidate={}
 for i=math.max(1,S.cursor-22),S.cursor do candidate[#candidate+1]=S.history[i]end
 candidate[#candidate+1]={url=url,entry=entry,anchor=false,page=1,anchors=false,font=S.font,reader=S.reader,layoutKey=false}
 return candidate,#candidate
end
local function restore(entry,mode,view,index)
 retainView();cancel();awaitDrain();W.stage("Reading cache")
 local ok,doc,err=pcall(function()
  local raw=cache:source(entry);if not raw then return nil,'Cache corrupt or unavailable'end
  local reader=S.reader;if view and view.reader~=nil then reader=view.reader end
  return M.parse(raw,entry.url,{reader=reader,css=cache:css(entry)})
 end)
 if not ok or not doc then restoreView();S.status='Offline parse failed: '..tostring(err or doc);return false end
 local font=view and view.font or S.font;local page=view and view.page or 1
 local anchors=view and (view.anchors or {view.anchor or {b=1,s=1,p=1}})or {{b=1,s=1,p=1}}
 W.stage('Preparing page');local prepared,next,done=firstPage(doc,anchors[page],page,font)
 remember();local candidate,cursor
 if mode=='push'then candidate,cursor=historyCandidate(entry.url,entry)end
 local status='Offline cache • '..doc.title
 W.check()
 W.atomic(function()
  if view and view.reader~=nil then S.reader=view.reader end;S.font=font
  S.doc,S.url,S.entry=doc,entry.url,entry;S.page,S.anchors=page,anchors
  if candidate then S.history,S.cursor=candidate,cursor elseif index then S.cursor=index end
  installPage(prepared,next,done);U.focus=1
  S.status=status;screen=nil
 end)
 if not S.rollbackView then cache:clearImages()end;return true
end
local open
local function fragment(target)
 if not S.doc or URL.wire(target)~=URL.wire(S.url)then return false end
 local id=target:match('#(.+)$');if not id then return false end
 id=id:gsub('%%(%x%x)',function(c)return string.char(tonumber(c,16))end)
 local anchor=S.doc.anchors and S.doc.anchors[id]
 if anchor then retainView();remember();reset(anchor);S.url=target;return true end
 S.status='Fragment not found';return true
end
open=function(target,mode,index)
 target=URL.resolve(S.url,target)
 if not target then S.status='Use a public HTTPS address (max 512 bytes)';redraw();return false end
 if fragment(target)then screen=nil;redraw();return true end
 cancel();pending={url=URL.wire(target),target=target,kind='page',mode=mode or 'push',index=index,generation=S.generation,reader=S.reader,font=S.font}
 if mode=='history'and S.history[index]then local view=S.history[index];if view.reader~=nil then pending.reader=view.reader end;pending.font=view.font or S.font end
 S.status='Queued • Back cancels';screen=nil;zoom=nil;redraw();return true
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
 if restore(e.entry,nil,e,index)then S.status='History • '..S.doc.title
 else open(e.url,'history',index)end
 screen=nil
end
local function turn(delta)
 if not S.doc then return end
 if delta>0 and S.next and not S.done then retainView();S.anchors[S.page+1]=S.next;S.page=S.page+1
 elseif delta<0 and S.page>1 then retainView();S.page=S.page-1 end
 redraw()
end
local function reparse()
 if not S.entry then return end
 local anchor=S.anchors[S.page]
 if restore(S.entry)then S.status='Mode changed • start of document'end
end
local function finish(body,job,css,candidate,event,discovery)
 local ok,doc,err=true,candidate,nil
 if not doc then ok,doc,err=pcall(M.parse,body,job.url,{reader=job.reader,css=css,discovery=discovery})end
 if not ok or not doc then release(event and event.handle);S.status='Page kept • '..tostring(err or doc);redraw();return end
 -- Cache before swap. A partial SD write cannot replace the visible document.
 W.stage('Preparing cache')
 local savedOk,entry,candidate=pcall(cache.save,cache,job.url,body,css,event,true)
 if not savedOk or not entry then release(event and event.handle);S.status=candidate=='publication_uncertain'and 'SD publication uncertain • page kept; check SD before reopening'or 'SD save failed • previous page kept';redraw();return end
 release(event and event.handle)
 local current=job.mode=='history'and S.history[job.index]or nil
 local anchors=current and (current.anchors or {current.anchor or {b=1,s=1,p=1}})or {{b=1,s=1,p=1}}
 local page=current and current.page or 1;local url=job.url
 if job.target and job.target:find('#',1,true)then
  local raw=job.target:match('#(.*)$');local id=raw:gsub('%%(%x%x)',function(c)return string.char(tonumber(c,16))end)
  local anchor=doc.anchors and doc.anchors[id]
  if anchor then url=job.url..'#'..raw;if not current then anchors={anchor};page=1 end end
 end
 W.stage('Preparing page');local prepared,next,done=firstPage(doc,anchors[page],page,job.font or S.font)
 retainView();remember();local newHistory,newCursor
 if job.mode~='history'then newHistory,newCursor=historyCandidate(job.url,entry)end
 local status=(doc.feed and 'Fetched feed • 'or 'Direct HTTPS • ')..(doc.truncated and 'text limit reached'or doc.cssLimited and 'CSS work limit • basic styles'or doc.title)
 W.stage('Publishing');W.check()
 if closed or job.generation~=S.generation then return end
 local committed,commitError=cache:commit(candidate,function()
  if newHistory then S.history,S.cursor=newHistory,newCursor else S.cursor=job.index end
  local current=S.history[S.cursor];if current then current.entry=entry end
  S.reader=job.reader;S.font=job.font or S.font
  S.doc,S.url,S.entry=doc,url,entry;S.page,S.anchors=page,anchors;U.focus=1
  installPage(prepared,next,done)
  S.status=status
 end)
 if not committed then S.status=commitError=='publication_uncertain'and 'SD publication uncertain • page kept; healthy reopen may recover new page'or 'SD save failed • previous page kept'end
 local status=S.status;W.stage('Cleaning cache');if committed and not S.rollbackView then cache:clearImages()end;cache:cleanup();S.status=status;redraw()
end
function on_init()
 startupComplete=false;launchChecked=false
 closed=false;active=nil;pending=nil;staged=nil;screen=nil;zoom=nil;menuPage=1
 S={status='Direct HTTPS • no scripts/logins/paywall bypass',page=1,anchors={{b=1,s=1,p=1}},reader=true,font=0,history={},cursor=0,generation=0}
 if not smudge.has_feature('browser_v6')then S.status='Please update firmware: Browser needs API 6 + PSRAM';closed=true;if smudge.fail_launch_url then smudge.fail_launch_url()end;redraw();return end
 queue(function()
 awaitDrain();cache=C.open(W,hasResources);W.stage('Reading cache');cache:cleanup();cache:clearImages();if #cache.sources==0 then cache.sources=defaults end
 if cache.entries[1]then restore(cache.entries[1],'push')end
 if cache.uncertain then S.status='SD cache read-only • check SD before reopening'end
 startupComplete=true
 end,'Reading cache')
 redraw()
end
local prepareLayout,receive
local function receiveCompletion(e)
 W.stage('Reading page')
 local ok,err=pcall(receive,e)
 if not ok then cancel();restoreView();S.status='Resource failed • page available: '..tostring(err);redraw()end
end
function on_update()
 if closed then return end
 if W.busy()then
  local done,err=W.step()
  if done==nil then cancel();if not startupComplete and smudge.fail_launch_url then smudge.fail_launch_url()end;S.status='Page kept • '..tostring(err);redraw()
  elseif not done then if S.workStage~=W.job.stage then S.workStage=W.job.stage;redraw()end
  else redraw()end
  return
 end
 -- Cache restore owns startup. Only then take the one-shot native payload,
 -- enqueue through normal navigation, and acknowledge that concrete queue entry.
 if startupComplete and not launchChecked then
  launchChecked=true
  if smudge.take_launch_url and smudge.ack_launch_url then
   local target,token=smudge.take_launch_url()
   if target then
    if type(target)=='string'and #target<=512 and target:sub(1,8)=='https://'and open(target)then
     if not smudge.ack_launch_url(token)then cancel();S.status='Cancelled • page kept';redraw()end
    elseif smudge.fail_launch_url then smudge.fail_launch_url()end
    return
   end
  end
 end
 if completion then
  local e=completion;completion=nil;W.start(function()receiveCompletion(e)end,'Reading page');return
 end
 if not active and not pending and imageRequest and smudge.http_busy and smudge.http_busy()then
  if S.status~='Waiting for cancelled request'then S.status='Waiting for cancelled request';redraw()end;return
 end
 if not active and not pending and not layoutRequest and imageRequest then
  local request=imageRequest;imageRequest=nil;preparingImage=request.image;local generation=S.generation
  W.start(function()
   imageSerial=imageSerial%999999+1
   local name='b6-pixels'..generation..'-'..imageSerial..'.pxc';local output=smudge.browser_path(name)
   for _=1,128 do if not smudge.file_exists(output)then break end;imageSerial=imageSerial%999999+1;name='b6-pixels'..generation..'-'..imageSerial..'.pxc';output=smudge.browser_path(name)end
   while true do
    local ok,err=smudge.browser_image_start(request.image.file,output,request.width,request.height)
    if ok then break end
    if err~='busy_draining'then request.image.error=err;return end
    W.stage('Waiting for cancelled image');W.check()
   end
   W.stage('Images')
   while true do
    local state=smudge.browser_image_poll()
    if state~=0 then
     if state==1 and generation==S.generation and not closed then
      -- Four bounded on-disk viewport caches; never a second framebuffer.
      if #decodedFiles>=4 then local old=table.remove(decodedFiles,1);smudge.delete_file(old.path);old.image.decoded[old.key]=nil end
      request.image.decoded=request.image.decoded or {};request.image.decoded[request.key]=output
      decodedFiles[#decodedFiles+1]={path=output,image=request.image,key=request.key}
     else request.image.error='Image decode failed or timed out'end
     break
    end
    W.check()
   end
   preparingImage=nil;redraw()
  end,'Images');return
 end
 if not active and not pending and layoutRequest then
  local request=layoutRequest;layoutRequest=nil;request.status=S.status
  W.start(function()awaitDrain();W.stage("Preparing page");prepareLayout(request)end,'Preparing page');return
 end
 if active or not pending then return end
 if smudge.browser_image_busy and smudge.browser_image_busy()then
  if S.status~='Waiting for cancelled image'then S.status='Waiting for cancelled image';redraw()end;return
 end
 local job=pending;pending=nil
 if not smudge.has_feature('https_async')then S.status='Live HTTPS unavailable on this host; use offline cache';redraw();return end
 local id,err=smudge.http_request{url=job.url,timeout_ms=30000,resource=job.kind,headers={Accept=job.kind=='image'and 'image/png,image/jpeg'or job.kind=='css'and 'text/css'or 'text/html,application/rss+xml,application/atom+xml'}}
 if id then active=job;active.id=id;S.status='Fetching '..job.kind..' • Back cancels'
 elseif err=='busy'then
  pending=job;if S.status~='Waiting for cancelled request'then S.status='Waiting for cancelled request';redraw()end;return
 else S.status='Request failed: '..tostring(err);staged=nil end
 redraw()
end
receive=function(e)
 if closed or not active or e.id~=active.id or active.generation~=S.generation then release(e.handle);return end
 local job=active;active=nil
 if e.error or e.status~=200 then
  if job.kind=='css'and staged then local stage=staged;staged=nil;finish(stage.body,stage.job,nil,nil,stage.event,stage.discovery)
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
   local stored,err
   repeat stored,err=smudge.http_store_step(e.handle,name);if stored==false then W.check()end until stored~=false
   if stored then image.file=path;image.error=nil else image.error=err or 'SD image store failed'end
  else image.error='Only valid JPEG/PNG resources are supported'end
  release(e.handle);S.status=image.file and 'Image downloaded • tap to enlarge'or image.error
 elseif job.kind=='css'then
  local stage=staged;staged=nil
  if stage then
   local css=e.body
   if e.handle then local parts={};for offset=0,e.size-1,8192 do local count=math.min(8192,e.size-offset);local bytes,err=smudge.http_read(e.handle,offset,count);if type(bytes)~='string'or #bytes~=count then error(err or 'storage_read')end;parts[#parts+1]=bytes end;css=table.concat(parts)end
   release(e.handle);finish(stage.body,stage.job,css,nil,stage.event,stage.discovery)
  else release(e.handle)end
 else
  if not cache then cache=C.open(W,hasResources)end
  local body=input(e)
  local ok,doc=pcall(M.discover,body,job.url)
  if ok and doc and #doc.stylesheets>0 then
   staged={body=body,job=job,event=e.handle and e or nil,discovery=doc};pending={url=doc.stylesheets[1],kind='css',generation=S.generation};S.status='Loading bounded stylesheet';doc=nil
  else finish(body,job,nil,nil,e.handle and e or nil,doc)end
 end
 redraw()
end
function on_http(e)
 if closed or not active or e.id~=active.id or active.generation~=S.generation then release(e.handle);return end
 if completion then if e.handle~=completion.handle then release(e.handle)end;return end
 if e.handle then owned[e.handle]=true end
 -- Keep the admitted handle owned while an ordinary menu/editor job drains.
 -- Navigation and Cancel invalidate this handoff through cancel().
 completion=e;redraw()
end

local function imageAction(image)
 if image.file then image.error=nil;zoom=zoom==image and nil or image;screen=nil;return end
 cancel();pending={url=image.url,kind='image',image=image,slot=1,generation=S.generation};S.status='Image queued • direct HTTPS';redraw()
end
local function formAction(f)
 form=f;screen='form';menuPage=1
end
local function saveBookmark()
 cancel();awaitDrain()
 if not S.url then return end
 if cache.uncertain then S.status='SD publication uncertain • check SD before reopening';return end
 for _,u in ipairs(cache.bookmarks)do if u==S.url then S.status='Already bookmarked';return end end
 if #cache.bookmarks>=20 then table.remove(cache.bookmarks,1)end
 cache.bookmarks[#cache.bookmarks+1]=S.url;local ok,err=cache:commit();S.status=ok and 'Bookmark saved'or err=='publication_uncertain'and 'SD publication uncertain • check SD before reopening'or 'Bookmark save failed'
end
local function find()
 U.edit('Find on page',findText or '',100,function(s)
  findText=s;local a=S.doc and L.find(S.doc,s,(S.anchors[S.page].b or 1)+1)
  if not a and S.doc then a=L.find(S.doc,s)end
  if a then retainView();reset(a);S.reflowTarget=a;screen=nil;S.status='Found: '..s else S.status='No match: '..s end
 end)
end
local function listActions()
 if not cache then return {{'Cancel loading',function()cancel();S.status='Cancelled • page kept'end}}end
 if screen=='news'then
  local rows={}
  for _,url in ipairs(cache.sources)do local target=url;rows[#rows+1]={target,function()open(target)end}end
  rows[#rows+1]={'Add feed URL',function()U.edit('HTTPS RSS / Atom feed','',512,function(s)local u=URL.resolve(nil,s);if u and #cache.sources<8 then cancel();awaitDrain();if cache.uncertain then S.status='SD publication uncertain • check SD before reopening';return end;cache.sources[#cache.sources+1]=u;local ok,err=cache:commit();if not ok then S.status=err=='publication_uncertain'and 'SD publication uncertain • check SD before reopening'or 'Feed save failed'end end end)end}
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
 {S.reader and 'Simplified page mode'or 'Reader mode',function()retainView();S.reader=not S.reader;reparse();screen=nil end},
 {'Text size: '..(S.font+1),function()retainView();local anchor=S.anchors[S.page];S.font=(S.font+1)%3;reset(anchor);S.reflowTarget=anchor;screen=nil end},
 {'Start of document',function()retainView();reset();screen=nil end},{'Find on page',find},{'Refresh page',function()if S.url then open(S.url)end end},
 {'Cancel loading',function()cancel();S.status='Cancelled • page kept';screen=nil end}}
end
prepareLayout=function(request)
 local anchor=request.anchor;local anchors,page
 if request.target then
  local target=request.target;anchor={b=1,s=1,p=1};anchors={anchor};page=1
  local function after(a,b)return a.b>b.b or a.b==b.b and (a.s>b.s or a.s==b.s and a.p>b.p)end
  for i=1,512 do
   local _,next,done=L.page(request.doc,anchor,request.width,request.height,request.font);page=i
   if done or after(next,target)or (next.b==anchor.b and next.s==anchor.s and next.p==anchor.p)then break end
   anchors[i+1]=next;anchor=next;W.check()
  end
 end
 local rows,next,done=L.page(request.doc,anchor,request.width,request.height,request.font)
 local prepared={doc=request.doc,page=anchors and page or request.page,key=request.key,rows=rows,sw=request.sw,sh=request.sh}
 W.atomic(function()
  if request.doc~=S.doc or request.page~=S.page then return end
  if anchors then S.anchors=anchors;S.page=page end
  S.next,S.done,S.layoutKey,S.reflowTarget=next,done,request.key,nil
  S.prepared=prepared;S.rollbackView=nil;S.status=request.status or S.status
 end);redraw()
end
local function preparedImage(image,x,y,width,height)
 local key=width..':'..height;local file=image.decoded and image.decoded[key]
 if file then return smudge.browser_image_blit(file,x,y,width,height)end
 if image.file and not image.error and not active and not pending and not W.busy()then imageRequest={image=image,key=key,width=width,height=height}end
 return false
end
function on_draw()
 if U.editor then U.editDraw();return end
 U.begin('Browser',U.fit(W.job and (W.job.stage..' • Back cancels')or S.status,220,'small'))
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 U.button('<',p,t,44,44,function()history(-1)end)
 U.button(S.url and U.fit(S.url:gsub('^https://',''),w-2*p-120)or 'Address / search',p+50,t,w-2*p-106,44,address)
 U.button('...',w-p-50,t,50,44,function()screen=screen and nil or 'menu';menuPage=1 end)
 local y=t+56;local bottom=b-66
 if closed then U.wrap(S.status,p,y,w-2*p,4);return end
 if zoom then
  U.button('Close image',p,y,w-2*p,44,function()zoom=nil end)
  if not preparedImage(zoom,p,y+52,w-2*p,math.max(1,bottom-y-52))then U.wrap('Image decoder rejected this image',p,y+60,w-2*p,3)end
 elseif screen then
  local actions=listActions();local n=math.max(1,(bottom-y-52)//50);local pages=math.max(1,math.ceil(#actions/n));menuPage=math.min(menuPage or 1,pages)
  for i=(menuPage-1)*n+1,math.min(menuPage*n,#actions)do local a=actions[i];U.button(U.fit(a[1],w-2*p-16),p,y+(i-(menuPage-1)*n-1)*50,w-2*p,44,a[2])end
  if pages>1 then U.button('More '..menuPage..'/'..pages,p,bottom-46,w-2*p,44,function()menuPage=menuPage%pages+1 end)end
  if #actions==0 then U.text(p,y,'Nothing saved yet')end
 elseif S.doc then
  local key=(w-2*p)..':'..(bottom-y)..':'..S.font
  local prepared=S.prepared
  if not prepared or prepared.doc~=S.doc or prepared.page~=S.page or prepared.key~=key then
   retainView();layoutRequest={sw=w,sh=U.h,doc=S.doc,page=S.page,key=key,width=w-2*p,height=bottom-y,font=S.font,target=S.reflowTarget or S.layoutKey and S.layoutKey~=key and S.anchors[S.page]or nil,anchor=S.anchors[S.page]}
  end
  local rows=prepared and prepared.doc==S.doc and prepared.key==key and prepared.page==S.page and prepared.rows or {}
  for _,row in ipairs(rows)do
   local yy=y+row.y
   if row.image then
    local img=row.image;local drawn=img.file and preparedImage(img,p,yy,w-2*p,row.height)
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
 if active or pending or W.busy()then U.button('Cancel',w-p-100,U.top+56,100,44,function()cancel();S.status='Cancelled • page kept';screen=nil end);U.buttons[#U.buttons].immediate=true end
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if key=='back'then if W.busy()then cancel()end;U.closeEditor()else U.key(key)end;redraw();return end
 if key=='back'then
  if active or pending or W.busy()then cancel();S.status='Cancelled • page kept'
  elseif zoom then zoom=nil elseif screen then screen=nil
  elseif S.cursor>1 then queue(function()history(-1)end,'Reading history')else closed=true;smudge.exit()end
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
function on_exit()remember();closed=true;cancel();layoutRequest=nil;if smudge.fail_launch_url then smudge.fail_launch_url()end end
