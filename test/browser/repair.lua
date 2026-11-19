local host=dofile;local modules={}
function dofile(n)local value=host((os.getenv('BROWSER_APP_ROOT')or 'apps/browser')..'/'..n);modules[n]=value;return value end
local resources={};local files={};local text='';local bodyText='';local request;local id=0;local fail=false;local w,h=480,800
smudge={api_version=5,has_feature=function()return true end,request_update=function()end,
 browser_path=function(n)return 'saved/'..n end,
 get_bounds=function()return w,h end,get_metrics=function()return{top_padding=10,header_height=40,button_hints_height=36,content_side_padding=16}end,
 text_width=function(s,font,style)return (utf8.len(s)or #s)*(font=='small'and 7 or font=='lexend16'and 13 or font=='lexend14'and 11 or 9)+(style=='bold'and 1 or 0)end,
 line_height=function(font)return font=='small'and 18 or font=='lexend16'and 34 or font=='lexend14'and 30 or 26 end,
 text=function(x,y,s,font)assert(x>=0 and y>=0 and x<=w and y+smudge.line_height(font)<=h,'text geometry '..s);text=text..s;bodyText=bodyText..s end,
 clear=function()text=''end,header=function(a,b)text=a..'\n'..b..'\n';bodyText=''end,button_hints=function()end,
 read_file=function(p,n,o)local value=files[p];if type(value)=='table'then return test_read(value.path,o or 0,n)end;return value and value:sub((o or 0)+1,(o or 0)+n)end,
 write_file=function(p,s,append)if fail then files[p]=(files[p]or '')..s:sub(1,2);return false end;files[p]=(append and files[p]or '')..s;return true end,
 http_read=function(handle,offset,count)assert(count<=8192);local value=resources[handle];if type(value)=='table'then return test_read(value.path,offset,count)end;return value and value:sub(offset+1,offset+count)end,
 http_release=function(handle)resources[handle]=nil;return true end,
 http_store=function(handle,name)if fail or files['saved/'..name]then return false end;files['saved/'..name]=assert(resources[handle]);return true end,
 file_exists=function(p)return files[p]~=nil end,delete_file=function(p)files[p]=nil end,
 list_files=function()local a={};for p in pairs(files)do a[#a+1]=p:gsub('^saved/','')end;return a end,
 in_rect=function(x,y,rx,ry,rw,rh)return x>=rx and y>=ry and x<rx+rw and y<ry+rh end,
 http_request=function(q)id=id+1;request={id=id,q=q};return id end,http_cancel=function()request=nil;return true end,
 browser_image=function()return false end,exit=function()end,
}
local imageJob
smudge.browser_image_start=function(source,output,width,height)imageJob={source=source,output=output,width=width,height=height};files[output]=files[source];return true end
smudge.browser_image_poll=function()return 1 end
smudge.browser_image_cancel=function()imageJob=nil end
smudge.browser_image_blit=function(path,...)return smudge.browser_image(path,...)end
smudge.http_store_step=function(...)if smudge.http_store(...)then return true end;return nil,'storage_copy'end
smudge.browser_resume=browser_native and browser_native.browser_resume
smudge.browser_atomic=browser_native and browser_native.browser_atomic
smudge.wrapped_text=function(s,width,font,style,n) local lines={};local line='';for word in s:gmatch('%S+')do if smudge.text_width(line..' '..word,font,style)>width and line~=''then lines[#lines+1]=line;line=''end;line=line==''and word or line..' '..word;if #lines>=(n or 100)then break end end;if #lines<(n or 100)and line~=''then lines[#lines+1]=line end;return{lines=lines}end
local function geom(x,y,rw,rh)assert(x>=0 and y>=0 and rw>0 and rh>0 and x+rw<=w+1 and y+rh<=h+1,'geometry '..x..','..y..','..rw..','..rh)end
smudge.rect=geom;smudge.rounded_rect=geom;smudge.rect_dither=geom;smudge.line=function()end
if font_width then
 smudge.text_width=font_width;smudge.browser_advance=font_advance;smudge.line_height=font_height
 local plain=smudge.text
 smudge.text=function(...)plain(...);font_text(...)end
 smudge.clear=function()text='';font_clear(w,h)end
 smudge.header=function(a,b)text=a..'\n'..b..'\n';bodyText='';font_text(16,12,a,'ui12','bold');font_text(w-16,15,b,'small','regular','right')end
 smudge.rect=function(x,y,rw,rh,filled)geom(x,y,rw,rh);font_rect(x,y,rw,rh,filled)end
 smudge.rounded_rect=function(x,y,rw,rh,r,filled)geom(x,y,rw,rh);font_rect(x,y,rw,rh,filled)end
end
local function capture(name)if font_capture and os.getenv('BROWSER_CAPTURE')then font_capture(os.getenv('BROWSER_CAPTURE')..'/'..name..'.pgm')end end
if font_image_data then smudge.browser_image=function(path,...)return files[path]and font_image_data(files[path],...)or false end end
local function draw()
 local idle=0
 for i=1,20000 do
  on_draw();on_update()
  if not modules['work.lua']or not modules['work.lua'].busy()then idle=idle+1 else idle=0 end
  if idle>=3 then on_draw();return end
 end
 error('browser work did not drain')
end
local function click(label)
 draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then on_tap(b[1]+b[3]//2,b[2]+b[4]//2);draw();return end end
 error('missing '..label..' in '..text)
end
local function edit(s)modules['ui.lua'].editor.value=s;click('Done')end
local function deliver(s,url)
 local r=assert(request);request=nil;local handle=r.id+1000;resources[handle]=s;local hash=type(s)=='table'and s.hash or modules['cache.lua'].sum(s);on_http{id=r.id,status=200,handle=handle,size=type(s)=='table'and s.size or #s,hash=hash,url=url or r.q.url};on_update();draw()
end

local case=os.getenv('BROWSER_REPAIR_CASE')or 'all'
local function init()dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/old');on_update();deliver('<p>STABLE PAGE</p>')end
local function navigate(url)click('...');click('Address');edit(url);on_update();assert(request)end
local function tap(label)
 on_draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]==label then on_tap(b[1]+1,b[2]+1);return end end;error('missing tap '..label)
end
if case=='all'or case=='LUA-1'then
 init();navigate('https://example.org/new');tap('...');assert(modules['work.lua'].busy())
 deliver('<p>NEW PAGE</p>');assert(text:find('Address',1,true),'menu action lost')
 on_button('back',true);draw();assert(text:find('NEW PAGE',1,true),'matching completion lost')
 -- Queue a real address submission before dispatching the old completion.
 navigate('https://example.org/superseded');local old=request
 tap('...');draw();click('Address');modules['ui.lua'].editor.value='https://example.org/latest';tap('Done')
 resources[9999]='<p>OBSOLETE PAGE</p>'
 on_http{id=old.id,status=200,handle=9999,size=18,hash=1,url=old.q.url};draw()
 assert(request.q.url=='https://example.org/latest','new URL lost');assert(not resources[9999],'pending event leaked')
 deliver('<p>LATEST PAGE</p>');assert(text:find('LATEST PAGE',1,true)and not text:find('OBSOLETE PAGE',1,true));assert(next(resources)==nil)
 navigate('https://example.org/cancelled');tap('...');local r=request
 resources[9998]='cancelled';on_http{id=r.id,status=200,handle=9998,size=9,hash=1};on_button('back',true);draw();assert(next(resources)==nil,'cancel leaked pending event')
 on_exit();print('PASS LUA-1 actual touch-before-completion, menu, new URL supersession, Cancel')
end
if case=='all'or case=='LUA-2'then
 files={};init();navigate('https://example.org/css-error');deliver('<link rel="stylesheet" href="/broken.css"><p>CANDIDATE PAGE</p>')
 local r=request;local read=smudge.http_read;smudge.http_read=function()return nil,'storage_read'end
 deliver('p {display:none}');smudge.http_read=read
 assert(next(resources)==nil,'failed CSS retained HTML owner');assert(text:find('STABLE PAGE',1,true),'old view lost')
 local c=modules['cache.lua'].open();assert(c.entries[1].url=='https://example.org/old','failed operation published cache')
 on_http{id=r.id,status=200,body='<p>STALE</p>'};draw();assert(not text:find('STALE',1,true));assert(next(resources)==nil)
 on_exit();print('PASS LUA-2 immediate whole-operation release, retained view, no stale cache publication')
end
if case=='all'or case=='LUA-3'then
 local C=dofile('cache.lua')
 for _,fault in ipairs({'before','after','readback'})do
  files={};local c=C.open();local old=assert(c:save('https://example.org/old','OLD'));local new,candidate=c:save('https://example.org/new','NEW',nil,nil,true);assert(new)
  local read,write=smudge.read_file,smudge.write_file
  smudge.write_file=function(p,s,a)if p:find('b5-index',1,true)then if fault=='before'then return false end;write(p,s,a);return fault~='after'end;return write(p,s,a)end
  smudge.read_file=function(p,...)if fault=='readback'and p:find('b5-index',1,true)then return nil end;return read(p,...)end
  local published=false;local ok,reason=c:commit(candidate,function()published=true end)
  assert(not ok and reason=='publication_uncertain','ambiguous write must be explicit');assert(not published and c.entries[1].file==old.file)
  c:cleanup();assert(files['saved/'..old.file]and files['saved/'..new.file],'uncertain roots deleted')
  local attempts=0;smudge.write_file=function(...)attempts=attempts+1;return write(...)end
  assert(not c:commit(candidate));assert(attempts==0,'uncertain operation silently retried')
  smudge.read_file,smudge.write_file=read,write
  local reopened=C.open();assert(reopened.entries[1].url==(fault=='before'and 'https://example.org/old'or 'https://example.org/new'));assert(reopened:read(old)=='OLD');assert(reopened:read(new)=='NEW')
 end
 -- Cancellation at the actual production prepublication yield preserves disk.
 files={};local c=C.open();local old=assert(c:save('https://example.org/old','OLD'));local _,candidate=c:save('https://example.org/new','NEW',nil,nil,true)
 local W=dofile('work.lua');c=C.open(W);W.start(function()c:commit(candidate)end);W.step();W.cancel();assert(C.open().entries[1].file==old.file)
 -- The app uses the explicit recovery warning while keeping the visible page.
 files={};init();navigate('https://example.org/uncertain');local read,write=smudge.read_file,smudge.write_file;local written=false
 smudge.write_file=function(p,...)local ok=write(p,...);if p:find('b5-index',1,true)then written=true end;return ok end
 smudge.read_file=function(p,...)if written and p:find('b5-index',1,true)then return nil end;return read(p,...)end
 deliver('<p>RECOVERABLE NEW PAGE</p>');smudge.read_file,smudge.write_file=read,write
 assert(text:find('publication uncertain',1,true),'UI hid recovery uncertainty');assert(text:find('STABLE PAGE',1,true));assert(next(resources)==nil)
 on_exit();dofile('main.lua');on_init();draw();assert(text:find('RECOVERABLE NEW PAGE',1,true),'reopen did not recover checked candidate');on_exit()
 print('PASS LUA-3 write failures before/after mutation, readback, protected roots, no retry, cancel, UI/reopen')
end

-- Continuing read failure is not a recovery boundary. Use the real B5 writer.
if case=='all'or case=='LUA-3-reopen'or case=='LUA-3-cleanup'or case=='LUA-3-ui'then
 local C=dofile('cache.lua');files={}
 local c=C.open();local old=assert(c:save('https://example.org/old','<p>ORIGINAL OLD</p>','p {color:black}'))
 local new,candidate=c:save('https://example.org/new','<p>ORIGINAL NEW</p>','p {color:white}',nil,true);assert(new)
 local read,write,delete,store=smudge.read_file,smudge.write_file,smudge.delete_file,smudge.http_store
 local unread=false
 smudge.read_file=function(p,...)if unread and p=='saved/b5-index1'then return nil end;return read(p,...)end
 -- Complete the NEW root write, then fail only its readback.
 smudge.write_file=function(p,...)local ok=write(p,...);if p=='saved/b5-index1'then unread=true end;return ok end
 local ok,err=c:commit(candidate);assert(not ok and err=='publication_uncertain')
 smudge.write_file=write
 local snapshot={};for p,s in pairs(files)do snapshot[p]=s end
 files['saved/b5-999.page']='orphan';files['saved/b5-image1.jpg']='image'
 snapshot['saved/b5-999.page']='orphan';snapshot['saved/b5-image1.jpg']='image'
 local writes,deletes=0,0
 smudge.write_file=function(...)writes=writes+1;return write(...)end
 smudge.http_store=function(...)writes=writes+1;return store(...)end
 smudge.delete_file=function(...)deletes=deletes+1;return delete(...)end
 local function preserved()
  assert(writes==0 and deletes==0,'unresolved cache mutated: writes='..writes..' deletes='..deletes)
  for p,s in pairs(snapshot)do assert(files[p]==s,'lost recovery bytes: '..p)end
 end
 if case=='all'or case=='LUA-3-reopen'then
  local reopened=C.open();assert(reopened.entries[1].file==old.file)
  local e,reason=reopened:save('https://example.org/third','THIRD');assert(not e and reason=='publication_uncertain','unsafe reopen allowed save')
  local published=false;local committed,reason=reopened:commit(candidate,function()published=true end)
  assert(not committed and reason=='publication_uncertain'and not published,'unsafe reopen allowed commit')
  reopened:cleanup();reopened:clearImages();preserved()
  -- Restored reads on this instance alone do not clear the latch.
  unread=false;assert(not reopened:commit(candidate));preserved();unread=true
  print('PASS LUA-3 continuing unreadable reopen blocks save/commit/GC; roots and page/CSS bytes preserved')
 end
 if case=='all'or case=='LUA-3-cleanup'then
  unread=false;local healthy=C.open();unread=true
  local e,reason=healthy:save('https://example.org/third','THIRD')
  assert(not e and reason=='publication_uncertain','cleanup read failure did not block following save')
  assert(not healthy:commit(candidate));healthy:clearImages();preserved()
  print('PASS LUA-3 cleanup-discovered root read failure blocks immediately following save and commit')
 end
 if case=='all'or case=='LUA-3-ui'then
  dofile('main.lua');on_init();draw()
  assert(text:find('SD cache read-only',1,true),'startup hid unresolved read-only cache')
  assert(text:find('ORIGINAL OLD',1,true),'readable fallback unavailable');preserved()
  click('...');click('Bookmark this page');assert(text:find('publication uncertain',1,true),'bookmark reported success');preserved()
  click('Bookmark this page');assert(text:find('publication uncertain',1,true),'failed bookmark treated as saved');preserved()
  click('Live news feeds');click('Add feed URL');edit('https://example.org/feed');assert(text:find('publication uncertain',1,true),'feed reported success');preserved()
  on_button('back',true);draw();navigate('https://example.org/third');deliver('<p>THIRD</p>')
  assert(text:find('publication uncertain',1,true),'page save hid uncertainty');assert(text:find('ORIGINAL OLD',1,true));preserved()
  on_exit();preserved()
  print('PASS LUA-3 actual startup/bookmark/network/exit callbacks report read-only recovery and preserve disk')
 end
 unread=false;smudge.read_file,smudge.write_file,smudge.delete_file,smudge.http_store=read,write,delete,store
 local recovered=C.open();assert(not recovered.uncertain and recovered.entries[1].file==new.file,'healthy reopen lost ORIGINAL NEW')
 assert(recovered:read(old)=='<p>ORIGINAL OLD</p>'and recovered:read(new)=='<p>ORIGINAL NEW</p>')
 assert(recovered:css(old)=='p {color:black}'and recovered:css(new)=='p {color:white}')
 -- Readable checksum-invalid root remains ordinary fallback recovery.
 files['saved/b5-index1']='readable corrupt B5';local fallback=C.open();assert(not fallback.uncertain and fallback.entries[1].file==old.file)
 assert(fallback:save('https://example.org/healthy','HEALTHY'),'readable corrupt root blocked healthy save')
 files={};assert(not C.open().uncertain);assert(C.open():save('https://example.org/empty','EMPTY'))
 print('PASS LUA-3 healthy reopen selects ORIGINAL NEW; OLD/CSS retained; readable corruption and empty startup writable')
end

if case=='all'or case=='UI-MENU'then
 files={};init()
 for _,size in ipairs({{800,480},{480,800}})do
  w,h=size[1],size[2];draw();click('...')
  local seen={};local pages=0
  repeat
   draw();local more;local count=0
   for _,b in ipairs(modules['ui.lua'].buttons)do
    assert(b[6]~='Previous'and b[6]~='Next','document pager on menu')
    if b[6]:match('^More ')then more=b else seen[b[6]]=true end
    if b[2]>=modules['ui.lua'].content(0).y and not b[6]:match('^More ')then count=count+1 end
   end
   if pages==0 and w>h then assert(count>=8,'landscape menu too sparse')end
   pages=pages+1
   if not more then break end
   on_tap(more[1]+1,more[2]+1);draw()
  until pages>=10 or text:find('More 1/',1,true)
  for _,label in ipairs({'Document details','Address / search','Bookmarks','Offline pages','History','Find on page','Cancel loading','Mode: Direct'})do assert(seen[label],'unreachable '..label)end
  on_button('back',true);draw()
 end
 on_exit();print('PASS UI-MENU all actions shared geometry and no document pagers')
end

if case=='all'or case=='UI-GEOMETRY'then
 for _,size in ipairs({{800,480},{480,800}})do
  w,h=size[1],size[2];files={};init()
  local L,U=modules['layout.lua'],modules['ui.lua'];local page=L.page;local count=0;local prepared
  L.page=function(doc,anchor,width,height,font)
   local r=U.content(font);assert(width==r.width and height==r.height,'prepared/drawn content geometry differs')
   count=count+1;local rows,nexta,done=page(doc,anchor,width,height,font);prepared=rows;return rows,nexta,done
  end
  navigate('https://example.org/geometry');deliver('<p>GEOMETRY SENTINEL '..string.rep('bounded prose ',90)..'</p>')
  assert(count==1,'adoption scheduled duplicate layout: '..count)
  local painted={};local native=smudge.text
  smudge.text=function(x,y,s,font,...)painted[#painted+1]={x=x,y=y,text=s};return native(x,y,s,font,...)end
  for i=1,3 do on_draw();on_update();assert(not text:find('Loading - previous page retained',1,true),'adopted page flashes loading')end
  smudge.text=native;assert(count==1,'stable draw requested extra layout')
  local r=U.content(0)
  for _,row in ipairs(prepared)do for _,run in ipairs(row.runs or {})do
   local seen=false;for _,p in ipairs(painted)do if p.text==run.text and p.x==r.x+run.x and p.y==r.y+row.y then seen=true;break end end
   assert(seen,'prepared row not drawn at its exact geometry')
  end end
  L.page=page;on_exit()
 end
 print('PASS UI-GEOMETRY exact prepared/drawn rows and one layout without post-adoption loading')
end

if case=='all'or case=='UI-FOCUS'then
 files={};init()
 for _,size in ipairs({{800,480},{480,800}})do
  w,h=size[1],size[2];draw();click('...');modules['ui.lua'].focus=12
  click('Offline pages');assert(modules['ui.lua'].focus==1,'new view retained stale focus')
  on_button('back',true);draw();click('...')
  local U=modules['ui.lua'];local more
  for i,b in ipairs(U.buttons)do if b[6]:match('^More ')then more=b;U.focus=i end end
  assert(more,'missing menu pager');on_button('confirm',true);draw();assert(U.focus==1,'menu paging retained old action focus')
  on_button('back',true);draw()
 end
 on_exit();print('PASS UI-FOCUS both orientations view change and Confirm menu paging own visible focus')
end
