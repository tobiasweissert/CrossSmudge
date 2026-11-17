-- Lua-first reader: explicit Assisted / Direct / Offline transports.
browser_launch_api=1 -- Native optional launch handshake; older installed packages fail closed.
local U,C,L,URL=dofile('ui.lua'),dofile('cache.lua'),dofile('layout.lua'),dofile('url.lua')
local direct
local M=setmetatable({}, {__index=function(_,k)direct=direct or dofile('model.lua');return direct[k]end})
local B=dofile('backend.lua')
local B7=dofile('b7cache.lua')
local PNG=dofile('png.lua')
local b7,restore7,chunk7,receive7,saveNext7
local backendRid,cancelRid,lastRetry,saveAll,lastFind
local function clock()return smudge.millis and smudge.millis()or math.floor(os.clock()*1000)end
local function elapsed(start)return (clock()-start)&0x7fffffff end
local S={status='Direct HTTPS • no scripts or logins',page=1,anchors={{b=1,s=1,p=1}},reader=true,font=0,history={},cursor=0,generation=0}
local Source=dofile('source.lua')
local W=dofile('work.lua')
local owned={}
local function hasResources()return next(owned)~=nil end
local layoutRequest,imageRequest,preparingImage
local imageSerial=0
local decodedFiles={}
local pendingDelete
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
local function clearPendingImage(image)
 if image and image.pending7 then if image.file then
   if smudge.browser_image_busy and smudge.browser_image_busy()then pendingDelete=image.file else smudge.delete_file(image.file)end
  end;image.file=nil;image.pending7=nil end
end
local function cancel()
 if backendRid then cancelRid=backendRid;backendRid=nil end
 saveAll=nil
 if pending and pending.image then pending.image.error='Image cancelled • tap to retry'end
 if active then if active.image then active.image.error='Image cancelled • tap to retry'end;smudge.http_cancel(active.id);active=nil end
 if not W.running()then W.cancel();restoreView()end
 layoutRequest=nil
 if imageRequest then clearPendingImage(imageRequest.image);imageRequest.image.error='Image cancelled • tap to retry'end
 if preparingImage then clearPendingImage(preparingImage);preparingImage.error='Image cancelled • tap to retry';preparingImage=nil;zoom=nil end
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
 if entry then entry.anchor=view.anchors[view.page];entry.page=view.page;if view.doc and view.doc.srp then entry.anchors=nil else entry.anchors=view.anchors end;entry.font=view.font;entry.reader=view.reader;entry.layoutKey=view.layoutKey;entry.chunk=view.doc and view.doc.srp and view.doc.srp.chunk;if view.doc and view.doc.srp then entry.anchors=nil;entry.entry=view.entry end end
end
-- Network and offline navigation retain the same bounded set of page anchors.
local function historyCandidate(url,entry)
 local candidate={}
 for i=math.max(1,S.cursor-22),S.cursor do candidate[#candidate+1]=S.history[i]end
 candidate[#candidate+1]={url=url,entry=entry,anchor=false,page=1,anchors=false,font=S.font,reader=S.reader,layoutKey=false}
 return candidate,#candidate
end
local function restore(entry,mode,view,index)
 if entry and entry.b7 then return restore7(entry,mode,view,index)end
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
 if not anchor and S.doc.srp and S.transport=='Offline'then
  for n=0,S.doc.srp.chunks-1 do local doc=b7:load(S.entry,n);local a=doc and doc.anchors[id];if a then chunk7(n,a);return true end;W.check()end
 end
 if not anchor and S.doc.srp and S.transport=='Assisted'then
  cancel();pending=B.job('locate',{doc=S.doc.srp.doc,fragment=id},{generation=S.generation});S.status='Locating fragment';return true
 end
 if anchor then retainView();remember();reset(anchor);S.url=target;return true end
 S.status='Fragment not found';return true
end
open=function(target,mode,index)
 target=URL.resolve(S.url,target)
 if not target then S.status='Use a public HTTPS address (max 512 bytes)';redraw();return false end
 if mode=='history'then local entry=S.history[index];if not entry or URL.wire(entry.url)~=URL.wire(target)then mode='push';index=nil end end
 if fragment(target)then screen=nil;redraw();return true end
 if S.transport=='Offline'then
  local e=cache and cache:lookup(URL.wire(target));if e then return restore(e,'push')end
  if b7 then for _,e in ipairs(b7.entries)do if e.url==URL.wire(target)then e.b7=true;return restore7(e,'push')end end end
  S.status='Offline • address not saved';redraw();return false
 end
 if S.transport=='Assisted'then
  if not B.configured()then S.status='Assisted unavailable • update firmware for public mode';return false end
  local rid=smudge.browser_request_id();if not rid then S.status='Request ID unavailable';return false end
  cancel();backendRid=rid
  local nav={url=URL.wire(target),target=target,mode=mode or 'push',index=index,font=S.font,reader=S.reader,start=clock(),generation=S.generation}
  if mode=='history'and S.history[index]then local view=S.history[index];if view.reader~=nil then nav.reader=view.reader end;nav.font=view.font or S.font end
  pending=B.job('open',{rid=rid,url=nav.url,mode=nav.reader and 'reader'or 'simplified',images='on-demand'},{nav=nav,generation=S.generation})
  lastRetry={target=target,mode=mode,index=index};S.status='Assisted queued • Back cancels';screen=nil;zoom=nil;redraw();return true
 end
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
 elseif e.entry and e.entry.b7 then S.status='History missing • select Assisted and Retry to fetch';lastRetry={target=e.url,mode='history',index=index}
 else open(e.url,'history',index)end
 screen=nil
end
local turn
turn=function(delta)
 if not S.doc then return end
 if delta>0 and S.next and not S.done then retainView();S.anchors[S.page+1]=S.next;S.page=S.page+1
 elseif delta<0 and S.page>1 then
  if S.doc.srp and not S.anchors[S.page-1]then
   if not W.running()then queue(function()turn(-1)end,'Previous page');return end
   local target=S.page-1;local anchors={{b=1,s=1,p=1}};local prepared,next,done;local page=1
   while page<=target do prepared,next,done=firstPage(S.doc,anchors[page],page,S.font);if done or page==target then break end;page=page+1;anchors[page]=next;W.check()end
   retainView();W.atomic(function()S.anchors,S.page=anchors,page;installPage(prepared,next,done)end)
  else retainView();S.page=S.page-1 end
 elseif S.doc.srp then local n=S.doc.srp.chunk+delta;if n>=0 and n<S.doc.srp.chunks then chunk7(n,nil,delta<0)end end
 redraw()
end
local function reparse()
 if S.doc and S.doc.srp then if S.transport=='Assisted'then open(S.url)else S.status='Select Assisted to change extraction mode'end;return end
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
local function chunkPage(doc,target,font,last)
 local anchors={{b=1,s=1,p=1}};local page=1;local prepared,next,done
 local function after(a,b)return a.b>b.b or a.b==b.b and (a.s>b.s or a.s==b.s and a.p>b.p)end
 while page<=8192 do
  local anchor=anchors[page];prepared,next,done=firstPage(doc,anchor,page,font)
  if done or not last and after(next,target)or next.b==anchor.b and next.s==anchor.s and next.p==anchor.p then break end
  page=page+1;anchors[page]=next;W.check()
 end
 return prepared,next,done,anchors,page
end
-- Only typed validated chunks reach the production font layout.
local function publish7(doc,entry,nav,anchor,last,candidate)
 local current=nav and nav.mode=='history'and S.history[nav.index]
 local font=current and current.font or nav and nav.font or S.font
 anchor=anchor or current and current.anchor or {b=1,s=1,p=1}
 local page=current and current.page or 1;local anchors=current and current.anchors or {[page]=anchor}
 local prepared,next,done=firstPage(doc,anchor,page,font)
 if last or not current and (anchor.b~=1 or anchor.s~=1 or anchor.p~=1)or current and current.layoutKey and current.layoutKey~=prepared.key then
  prepared,next,done,anchors,page=chunkPage(doc,anchor,font,last)
 end
 if nav and nav.generation~=S.generation then return end
 retainView();remember();local hist,cursor
 entry.b7=true
 if nav and nav.mode~='history'then hist,cursor=historyCandidate(doc.srp.url,entry)end
 local n,total=b7:coverage(entry);local status='Chunk '..(doc.srp.chunk+1)..'/'..doc.srp.chunks..' • '..n..'/'..total..' saved'..(doc.truncated and ' • truncated'or '')
 W.check();local function apply()
  if hist then S.history,S.cursor=hist,cursor elseif current then S.cursor=nav.index;current.entry=entry end
  S.doc,S.url,S.entry=doc,doc.srp.url,entry;S.font=font;S.reader=doc.srp.mode=='reader';S.page,S.anchors=page,anchors
  installPage(prepared,next,done);screen=nil;zoom=nil
  S.status=status
 end
 if candidate then local ok,err=b7:commit(candidate,apply);if not ok then S.status=err=='publication_uncertain'and 'SD cache read-only • page kept'or 'Assisted save failed • '..tostring(err)..' • page kept';return false end else W.atomic(apply)end
 return true
end
restore7=function(entry,mode,view,index)
 cancel();awaitDrain();local latest=b7:lookup(entry.doc)or entry
 local n=view and view.chunk or 0;local doc=b7:load(latest,n)
 if not doc then S.status='Saved chunk unavailable • page kept';return false end
 publish7(doc,latest,{mode=mode or 'history',index=index,font=view and view.font or S.font,generation=S.generation},view and view.anchor)
 return true
end
chunk7=function(n,anchor,last)
 local srp=S.doc and S.doc.srp;if not srp then return end
 local entry=b7:lookup(srp.doc)or S.entry;local doc=b7:load(entry,n)
 if doc then publish7(doc,entry,nil,anchor,last);return end
 if S.transport~='Assisted'then S.status='Chunk not saved • select Assisted to fetch';return end
 cancel();pending=B.job('chunk',{doc=srp.doc,chunk=n},{generation=S.generation,anchor=anchor,last=last});S.status='Loading chunk '..(n+1)
end
saveNext7=function()
 if not saveAll then return end
 local entry=b7:lookup(saveAll.doc);if not entry then saveAll=nil;return end
 while saveAll.next<entry.chunks and b7:load(entry,saveAll.next)do saveAll.next=saveAll.next+1;W.check()end
 if saveAll.next>=entry.chunks then saveAll=nil;S.entry=entry;entry.b7=true;S.status='Saved complete • all chunks (images on demand)';return end
 if S.transport~='Assisted'then saveAll=nil;S.status='Partial save • select Assisted for missing chunks';return end
 pending=B.job('chunk',{doc=entry.doc,chunk=saveAll.next},{generation=S.generation,saving=true});S.status='Saving chunk '..(saveAll.next+1)..'/'..entry.chunks
end
receive7=function(e,job)
 if job.endpoint=='cancel'then if e.handle then pcall(B.read,e)end;release(e.handle);return end
 if e.error then if job.image then job.image.error='Image '..e.error..' • tap to retry'end;release(e.handle);S.status='Assisted '..e.error..' • page kept; Retry or choose mode';return end
 if job.endpoint=='image'and e.status==200 then
  local ok=e.handle and e.content_type=='image/png'and PNG.validate(function(offset,count)return smudge.http_read(e.handle,offset,count)end,e.size,job.payload.width,job.payload.height)
  if not ok then release(e.handle);job.image.error='Invalid assisted PNG';S.status=job.image.error;return end
  local name='b7-download'..S.generation..'.png';local serial=0
  while smudge.file_exists(smudge.browser_path(name))and serial<128 do serial=serial+1;name='b7-download'..S.generation..'-'..serial..'.png'end
  local done,err;repeat done,err=smudge.http_store_step(e.handle,name);if done==false then W.check()end until done~=false
  release(e.handle)
  if not done then job.image.error=err or 'PNG save failed';return end
  job.image.file=smudge.browser_path(name);job.image.error=nil;job.image.pending7={entry=job.entry,asset=job.image.asset}
  imageRequest={image=job.image,key=job.payload.width..':'..job.payload.height,width=job.payload.width,height=job.payload.height};S.status='Decoding assisted PNG';return
 end
 local ok,value,raw=pcall(B.read,e);release(e.handle)
 if not ok then if job.image then job.image.error='Invalid image response • tap to retry'end;S.status='Invalid assisted response • page kept';return end
 if value.error then if job.image then job.image.error='Image '..value.error..' • tap to retry'end;saveAll=nil;S.status='Assisted '..value.error..' • page kept; Retry or choose mode';return end
 if job.endpoint=='cancel'then return end
 if value.state=='pending'then
  if not job.nav then if job.image then job.image.error='Unexpected image response'end;S.status='Unexpected pending response';return end
  pending=B.job('poll',{job=value.job},{nav=job.nav,generation=S.generation,due=value.retry_ms,waitStart=clock()});S.status='Assisted converting • Back cancels';return
 end
 if job.endpoint=='locate'or job.endpoint=='find'then
  if job.endpoint=='find'then lastFind=value.found and {doc=job.payload.doc,text=job.payload.text,position={chunk=value.chunk,block=value.block,run=value.run,byte=value.byte}}or nil end
  if not value.found then S.status=job.endpoint=='find'and 'No match in document'or 'Fragment not found';return end
  local srp=S.doc and S.doc.srp;if not srp or job.payload.doc~=srp.doc or value.chunk>=srp.chunks then S.status='Invalid assisted position';return end
  local entry=b7:lookup(srp.doc);local doc=b7:load(entry,value.chunk)
  if doc then local valid,a=pcall(B.protocol.anchor,doc,value);if valid then publish7(doc,entry,nil,a)else S.status='Invalid assisted position'end
  elseif S.transport=='Assisted'then pending=B.job('chunk',{doc=srp.doc,chunk=value.chunk},{generation=S.generation,position=value})end
  return
 end
 if not value.srp then S.status='Unexpected assisted response';return end
 local t=value.srp
 if job.endpoint=='chunk'and (t.doc~=job.payload.doc or t.chunk~=job.payload.chunk)or job.nav and (t.chunk~=0 or t.mode~=(job.nav.reader and 'reader'or 'simplified'))then S.status='Mismatched assisted chunk';return end
 local anchor=job.anchor
 if job.position then local valid,a=pcall(B.protocol.anchor,value,job.position);if not valid then S.status='Invalid assisted position';return end;anchor=a end
 if not b7 then b7=B7.open(W)end;if not cache then cache=C.open(W,hasResources)end
 local entry,err=b7:save(value,raw,not job.saving)
 if not entry then S.status=err=='publication_uncertain'and 'SD cache read-only • page kept'or 'Assisted save failed • '..tostring(err)..' • page kept';return end
 if job.saving then S.entry=entry;entry.b7=true;saveAll.next=t.chunk+1;saveNext7();return end
 if not publish7(value,entry,job.nav,anchor,job.last,err)then return end
 if job.nav then
  backendRid=nil
  local frag=job.nav.target and job.nav.target:match('#(.+)$')
  if frag then pending=B.job('locate',{doc=t.doc,fragment=frag:gsub('%%(%x%x)',function(c)return string.char(tonumber(c,16))end)},{generation=S.generation})end
 end
end
function on_init()
 startupComplete=false;launchChecked=false
 closed=false;active=nil;pending=nil;staged=nil;screen=nil;zoom=nil;menuPage=1
 S={transport=B.configured()and 'Assisted'or 'Direct',status=B.available()and 'Public Assisted • shared capacity; not private storage'or 'Direct HTTPS • Assisted needs firmware update',page=1,anchors={{b=1,s=1,p=1}},reader=true,font=0,history={},cursor=0,generation=0}
 if not smudge.has_feature('browser_v6')then S.status='Please update firmware: Browser needs API 6 + PSRAM';closed=true;if smudge.fail_launch_url then smudge.fail_launch_url()end;redraw();return end
 queue(function()
 awaitDrain();b7=B7.open(W);b7:objects();cache=C.open(W,hasResources);W.stage('Reading cache');cache:cleanup();cache:clearImages();if #cache.sources==0 then cache.sources=defaults end
 if b7.entries[1]then b7.entries[1].b7=true;restore7(b7.entries[1],'push')elseif cache.entries[1]then restore(cache.entries[1],'push')end
 if cache.uncertain or b7.uncertain then S.status='SD cache read-only • check SD before reopening'end
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
 if pendingDelete and not(smudge.browser_image_busy and smudge.browser_image_busy())then smudge.delete_file(pendingDelete);pendingDelete=nil end
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
      if request.image.pending7 then
       local meta=request.image.pending7;local bytes=smudge.read_file(request.image.file,65537);local latest=b7:lookup(meta.entry.doc);if bytes and latest then local entry,path=b7:image(latest,meta.asset,bytes);if entry then if S.entry and S.entry.doc==entry.doc then entry.b7=true;S.entry=entry end;smudge.delete_file(request.image.file);request.image.file=path else clearPendingImage(request.image);request.image.error='Image cache publication failed'end end;request.image.pending7=nil
      end
      request.image.decoded=request.image.decoded or {};request.image.decoded[request.key]=output
      decodedFiles[#decodedFiles+1]={path=output,image=request.image,key=request.key}
     else clearPendingImage(request.image);request.image.error='Image decode failed or timed out'end
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
 if not active and not pending and cancelRid and S.transport=='Assisted'and B.configured()then pending=B.job('cancel',{rid=cancelRid},{generation=S.generation});cancelRid=nil end
 if active or not pending then return end
 if pending.due and elapsed(pending.waitStart)<pending.due then return end
 if pending.nav and elapsed(pending.nav.start)>=30000 then cancel();S.status='Assisted timeout • page kept';redraw();return end
 if smudge.browser_image_busy and smudge.browser_image_busy()then
  if S.status~='Waiting for cancelled image'then S.status='Waiting for cancelled image';redraw()end;return
 end
 local job=pending;pending=nil
 if not smudge.has_feature('https_async')then S.status='Live HTTPS unavailable on this host; use offline cache';redraw();return end
 local id,err
 if job.kind=='backend'then id,err=B.request(job)else id,err=smudge.http_request{url=job.url,timeout_ms=30000,resource=job.kind,headers={Accept=job.kind=='image'and 'image/png,image/jpeg'or job.kind=='css'and 'text/css'or 'text/html,application/rss+xml,application/atom+xml'}}end
 if id then active=job;active.id=id;if job.endpoint~='cancel'then S.status='Fetching '..job.kind..' • Back cancels'end
 elseif err=='busy'then
  pending=job;if job.endpoint~='cancel'and S.status~='Waiting for cancelled request'then S.status='Waiting for cancelled request';redraw()end;return
 else if job.endpoint~='cancel'then S.status='Request failed: '..tostring(err);staged=nil end end
 redraw()
end
receive=function(e)
 if closed or not active or e.id~=active.id or active.generation~=S.generation then release(e.handle);return end
 local job=active;active=nil
 if job.kind=='backend'then receive7(e,job);return end
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
 if image.asset and not image.file then
  if S.transport~='Assisted'then S.status='Image not saved • select Assisted to load';return end
  cancel();image.error='Loading PNG';pending=B.job('image',{doc=S.doc.srp.doc,asset=image.asset,width=math.min(800,U.w-2*U.pad),height=180},{generation=S.generation,image=image,entry=S.entry});S.status='Loading assisted PNG';return
 end
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
  findText=s
  if S.doc and S.doc.srp and S.transport=='Assisted'then
   cancel();pending=B.job('find',{doc=S.doc.srp.doc,text=s,after=lastFind and lastFind.doc==S.doc.srp.doc and lastFind.text==s and lastFind.position or B.json.null},{generation=S.generation});S.status='Finding in document';return
  end
  if S.doc and S.doc.srp then
   local after=lastFind and lastFind.doc==S.doc.srp.doc and lastFind.text==s and lastFind.position
   for n=after and after.chunk or 0,S.entry.chunks-1 do
    local doc=b7:load(S.entry,n)
    if doc and s~=''then
     -- SRP1 bounds the entire chunk's decoded text to 8192 bytes. Find is a
     -- cold action: one block scratch string lets styles cross without joining
     -- paragraphs or retaining another chunk/document index in the Lua heap.
     local needle=s:gsub('[A-Z]',string.lower)
     local parts={}
     for i,block in ipairs(doc.blocks)do
      for j,run in ipairs(block.spans)do parts[j]=run.text end
      local folded=table.concat(parts):gsub('[A-Z]',string.lower)
      local pos=1
      if after and n==after.chunk then
       if block.id<after.block then pos=#folded+1
       elseif block.id==after.block then
        local offset=0
        for j,run in ipairs(block.spans)do
         if j<after.run then offset=offset+#run.text
         elseif j==after.run then pos=offset+(utf8.offset(run.text,2,after.byte)or #run.text+1);break end
        end
       end
      end
      local at=folded:find(needle,pos,true)
      if at then
       local offset=0
       for j,run in ipairs(block.spans)do
        if at<=offset+#run.text then
         local byte=at-offset
         lastFind={doc=doc.srp.doc,text=s,position={chunk=n,block=block.id,run=j,byte=byte}}
         -- Reuse the validated candidate instead of loading a third chunk model.
         publish7(doc,S.entry,nil,{b=i,s=j,p=byte});S.status='Found in saved chunks • partial search';return
        end
        offset=offset+#run.text
       end
      end
      for j=1,#parts do parts[j]=nil end
      W.check()
     end
    end;W.check()
   end
   lastFind=nil
   S.status='No match in saved chunks • partial search';return
  end
  local a=S.doc and L.find(S.doc,s,(S.anchors[S.page].b or 1)+1)
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
  if screen=='offline'and b7 then for _,e in ipairs(b7.entries)do local item=e;local n,total=b7:coverage(e);rows[#rows+1]={e.title..' • '..n..'/'..total..' chunks',function()item.b7=true;restore7(item,'push')end}end end
  return rows
 elseif screen=='form'then
  local rows={}
  for _,f in ipairs(form.fields)do if not f.hidden then local field=f;rows[#rows+1]={f.name..': '..f.value,function()U.edit(field.name,field.value,256,function(s)field.value=s end)end}end end
  rows[#rows+1]={'Submit GET search',function()local url;if S.doc and S.doc.srp then url=B.protocol.formURL(form)else url=M.formURL(form)end;if url then open(url)else S.status='Search URL exceeds limits'end end};return rows
 end
 local modes={{'Mode: '..(S.transport or 'Direct'),function()screen='modes';menuPage=1 end},{'Retry last navigation',function()if lastRetry then open(lastRetry.target,lastRetry.mode,lastRetry.index)elseif S.url then open(S.url)end end},{'Save offline (all chunks)',function()if S.doc and S.doc.srp then cancel();saveAll={doc=S.doc.srp.doc,next=0};saveNext7()else S.status='Direct pages are saved after loading'end;screen=nil end}}
 if screen=='modes'then return {{'Assisted'..(B.configured()and ''or ' • unavailable'),function()if B.configured()then cancel();S.transport='Assisted';S.status='Public Assisted • shared capacity; not private storage';screen=nil else S.status='Update firmware for public Assisted mode'end end},{'Direct',function()cancel();S.transport='Direct';cancelRid=nil;S.status='Direct HTTPS';screen=nil end},{'Offline',function()cancel();S.transport='Offline';cancelRid=nil;S.status='Offline • saved coverage only';screen=nil end}}end
 local rows={{'Document details',function()screen='details';menuPage=1 end},{'Address / search',address},{'Back website',function()history(-1)end},{'Forward website',function()history(1)end},
 {'Live news feeds',function()screen='news';menuPage=1 end},{'Bookmarks',function()screen='bookmarks';menuPage=1 end},
 {'Bookmark this page',saveBookmark},{'Offline pages',function()screen='offline';menuPage=1 end},{'History',function()screen='history';menuPage=1 end},
 {S.reader and 'Simplified page mode'or 'Reader mode',function()retainView();S.reader=not S.reader;reparse();screen=nil end},
 {'Text size: '..(S.font+1),function()retainView();local anchor=S.anchors[S.page];S.font=(S.font+1)%3;reset(anchor);S.reflowTarget=anchor;screen=nil end},
 {'Start of document',function()if S.doc and S.doc.srp then chunk7(0)else retainView();reset()end;screen=nil end},{'Find on page',find},{'Refresh page',function()if S.url then open(S.url)end end},
 {'Cancel loading',function()cancel();S.status='Cancelled • page kept';screen=nil end}}
 for _,row in ipairs(modes)do rows[#rows+1]=row end;return rows
end
prepareLayout=function(request)
 local anchor=request.anchor;local anchors,page
 if request.target then
  local target=request.target;anchor={b=1,s=1,p=1};anchors={anchor};page=1
  local function after(a,b)return a.b>b.b or a.b==b.b and (a.s>b.s or a.s==b.s and a.p>b.p)end
  for i=1,request.doc.srp and 8192 or 512 do
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
 local width=smudge.get_bounds();local metrics=smudge.get_metrics();local pad=math.max(12,metrics.content_side_padding or 20)
 local subtitle=(S.transport or 'Direct')..' • '..(W.job and (W.job.stage..' • Back cancels')or S.status)
 U.begin('Browser',U.fit(subtitle,math.max(80,width-smudge.text_width('Browser','ui12','bold')-3*pad),'small'))
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 U.button('<',p,t,44,44,function()history(-1)end)
 U.button(S.url and U.fit(S.url:gsub('^https://',''),w-2*p-120)or 'Address / search',p+50,t,w-2*p-106,44,address)
 U.button('...',w-p-50,t,50,44,function()screen=screen and nil or 'menu';menuPage=1 end)
 local y=t+56;local bottom=b-66
 if closed then U.wrap(S.status,p,y,w-2*p,4);return end
 if zoom then
  U.button('Close image',p,y,w-2*p,44,function()zoom=nil end)
  if not preparedImage(zoom,p,y+52,w-2*p,math.max(1,bottom-y-52))then U.wrap('Image decoder rejected this image',p,y+60,w-2*p,3)end
 elseif screen=='details'then
  local title=S.doc and S.doc.title or 'No document loaded';local yy=U.wrap(title,p,y,w-2*p,math.max(1,(bottom-y-60)//smudge.line_height('lexend14')),'lexend14',true)
  if S.doc and S.doc.srp then local n,total=b7:coverage(S.entry);U.wrap(n..'/'..total..' chunks stored • '..S.doc.srp.mode..' • '..S.doc.srp.lang..(S.doc.srp.truncated and ' • truncated: '..(S.doc.srp.reason or 'limit')or ''),p,yy+8,w-2*p,2)end
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
    if not drawn then smudge.rect(p,yy,w-2*p,row.height,false);if img.asset then U.wrap(img.alt..' • tap to load PNG',p+8,yy+8,w-2*p-16,1);if img.error then U.wrap(img.error,p+8,yy+8+smudge.line_height('ui12'),w-2*p-16,1)end else U.wrap((img.error or img.alt)..' • tap to load JPEG/PNG',p+8,yy+8,w-2*p-16,2)end end
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
  U.wrap(S.transport=='Assisted'and 'Public Assisted reader. Shared capacity and short-lived extraction cache, not private storage. The backend and hosting operator see addresses/content; opaque handles are not login protection. No scripts, logins or paywall bypass.'or S.transport=='Offline'and 'Offline reader. Choose saved pages; missing chunks and images need an explicit switch to Assisted.'or 'Direct public HTTPS browser. Reader mode, RSS/Atom news, bookmarks and checked offline pages. JavaScript, logins, paywalls and bot challenges are unsupported.',p,y,w-2*p,6)
  U.button('Live news feeds',p,bottom-100,w-2*p,44,function()screen='news';menuPage=1 end)
  U.button('Offline pages',p,bottom-50,w-2*p,44,function()screen='offline';menuPage=1 end)
 end
 local bw=(w-2*p-16)//3
 U.button('Previous',p,b-50,bw,44,function()turn(-1)end)
 U.button((S.doc and S.doc.srp and 'P'..S.page..' C'..(S.doc.srp.chunk+1)..'/'..S.doc.srp.chunks or 'Page '..S.page..(S.done and ' • end'or '')),p+bw+8,b-50,bw,44,function()screen='menu';menuPage=1 end)
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
