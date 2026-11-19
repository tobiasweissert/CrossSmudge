-- Reuses the existing Browser input/SD/geometry fixture with synthetic SRP1.
local host=dofile;local modules={}
function dofile(n)local value=host('apps/browser/'..n);modules[n]=value;return value end
local resources={};local files={};local text='';local bodyText='';local request;local id=0;local fail=false;local failRoot=false;local w,h=480,800
smudge={api_version=5,has_feature=function()return true end,request_update=function()end,
 browser_path=function(n)return 'saved/'..n end,
 get_bounds=function()return w,h end,get_metrics=function()return{top_padding=10,header_height=40,button_hints_height=36,content_side_padding=16}end,
 text_width=function(s,font,style)return (utf8.len(s)or #s)*(font=='small'and 7 or font=='lexend16'and 13 or font=='lexend14'and 11 or 9)+(style=='bold'and 1 or 0)end,
 line_height=function(font)return font=='small'and 18 or font=='lexend16'and 34 or font=='lexend14'and 30 or 26 end,
 text=function(x,y,s,font)assert(x>=0 and y>=0 and x<=w and y+smudge.line_height(font)<=h,'text geometry '..s);text=text..s;bodyText=bodyText..s end,
 clear=function()text=''end,header=function(a,b)text=a..'\n'..b..'\n';bodyText=''end,button_hints=function()end,
 read_file=function(p,n,o)local value=files[p];if type(value)=='table'then return test_read(value.path,o or 0,n)end;return value and value:sub((o or 0)+1,(o or 0)+n)end,
 write_file=function(p,s,append)if fail or failRoot and p:find('b7-index',1,true)then files[p]=(files[p]or '')..s:sub(1,2);return false end;files[p]=(append and files[p]or '')..s;return true end,
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
 smudge.header=function(a,b)assert(font_width(a,'ui12','bold')+font_width(b,'small','regular')<=w-32,'header text overlaps');text=a..'\n'..b..'\n';bodyText='';font_text(16,12,a,'ui12','bold');font_text(w-16,15,b,'small','regular','right')end
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

local J=dofile('json.lua')
smudge.api_version=7
local configured=false
local imports=0
smudge.browser_backend_configured=function()error('app must not inspect credentials')end
smudge.browser_backend_configure=function()error('app must not import private file')end
smudge.browser_request_id=function()return string.rep('c',32)end
local now=0;smudge.millis=function()return now end
smudge.browser_backend_request=function(q)id=id+1;request={id=id,q=q};return id end
local function deliver7(value,status)
 local r=assert(request);request=nil;local handle=r.id+1000;local s=type(value)=='string'and value or J.encode(value);resources[handle]=s
 on_http{id=r.id,status=status or 200,handle=handle,size=#s,hash=1,content_type='application/json'};on_update();draw()
end
local function ready(n,title)
 return {v=1,state='ready',doc=string.rep('a',32),url='https://example.org/one',title=title or 'Original synthetic story',lang='en',dir='ltr',mode='reader',chunks=2,chunk=n,truncated=false,
 blocks=J.array{{id=n*10+1,kind='h1',runs=J.array{{text=n==0 and 'Original opening'or 'Original ending',size=2,bold=true}}},{id=n*10+2,kind='p',runs=J.array{{text='Read the complete original synthetic story. '},{text='Adjacent A',href='https://example.org/a'},{text='Adjacent B',href='https://example.org/b'}}}},
 anchors=J.array{{id=n==0 and 'top'or 'tail',block=n*10+1,run=1,byte=1}}}
end
local function menu(label)
 click('...')
 for _=1,10 do
  draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then click(label);return end end
  click('More')
 end;error('missing menu '..label)
end
local function endpoint(n)assert(request and request.q.path=='/v1/'..n,text..' expected '..n);return J.decode(request.q.body)end
local function cancelServer()
 draw();if request and request.q.path=='/v1/cancel'then deliver7({v=1,state='cancelled'})end
end
dofile('main.lua');on_init();draw();assert(not modules['model.lua'],'Direct parser must be lazy');assert(imports==0 and not request,'startup imported or forwarded configuration');assert(not text:find('Direct HTTPS',1,true),'public default must not advertise Direct transport');assert(not text:find('Import server configuration',1,true),'retired setup UI')
click('Address');edit('https://example.org/one');on_update();assert(endpoint('open').rid==string.rep('c',32))
deliver7({v=1,state='pending',job=string.rep('b',32),retry_ms=500},202);assert(not request)
now=499;on_update();assert(not request);now=500;on_update();endpoint('poll');deliver7(ready(0));assert(text:find('Original opening',1,true),text);assert(text:find('Chunk 1/2',1,true),text);capture('assisted-opening-portrait')
-- Adjacent 44px links never overlap.
local hits={};for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Adjacent A'or b[6]=='Adjacent B'then hits[#hits+1]=b end end
assert(#hits==2,'both adjacent links visible');if #hits==2 then local a,b=hits[1],hits[2];assert(a[1]+a[3]<=b[1]or b[1]+b[3]<=a[1]or a[2]+a[4]<=b[2]or b[2]+b[4]<=a[2])end
on_button('page_forward',true);draw();on_update();assert(endpoint('chunk').chunk==1);deliver7(ready(1));assert(text:find('Original ending',1,true));capture('assisted-ending-portrait')
on_button('page_back',true);draw();assert(text:find('Original opening',1,true));assert(not request)
-- Remote find resolves a global block into the saved later chunk.
menu('Find on page');edit('ending');on_update();endpoint('find');deliver7({v=1,found=true,chunk=1,block=11,run=1,byte=1});assert(text:find('Original ending',1,true))
-- Bad JSON/unknown fields cannot replace the readable page.
menu('Refresh page');on_update();endpoint('open');deliver7('{"v":1,"state":"ready","unknown":true}');assert(text:find('Original ending',1,true),text);assert(text:find('Invalid assisted',1,true),text)
menu('Refresh page');cancelServer();on_update();if not request then on_update()end
if request and request.q.path=='/v1/cancel'then cancelServer();on_update()end
endpoint('open');local stale=request;request=nil;on_button('back',true);resources[999]='{}';on_http{id=stale.id,handle=999,size=2,status=200};draw();assert(not resources[999]);cancelServer();assert(text:find('Original ending',1,true))
-- Explicit Offline must not start a request on paging or Find; B7 survives reopen.
menu('Mode:');click('Offline');assert(not request,request and request.q.path);menu('Find on page');edit('opening');assert(not request,request and request.q.path);assert(text:find('opening',1,true),text)
on_exit();dofile('main.lua');on_init();draw();assert(text:find('Original opening',1,true));assert(not request,'startup must never forward saved URL')
-- Both orientations preserve source identity, geometry, local reflow.
w,h=800,480;draw();capture('assisted-opening-landscape');w,h=480,800;draw()
-- Save all sequentially checks missing chunks, never says complete early.
menu('Refresh page');on_update();endpoint('open');local t=ready(0);t.doc=string.rep('d',32);deliver7(t)
menu('Save offline');on_update();assert(endpoint('chunk').chunk==1);t=ready(1);t.doc=string.rep('d',32);deliver7(t);assert(text:find('Saved complete',1,true))
-- Typed GET forms and opaque PNG assets use the selected assisted lane.
-- This legacy fixture specifically exercises manual failure/retry behavior.
menu('Images: Automatic')
menu('Refresh page');on_update();endpoint('open');local feature=ready(0);feature.doc=string.rep('f',32);feature.chunks=1
feature.blocks=J.array{{id=1,kind='h1',runs=J.array{{text='Original forms and image'}}},{id=2,kind='form',form={action='https://example.org/search',fields=J.array{{name='q',value='a & b',type='search'},{name='lang',value='en',type='hidden'}}}},{id=3,kind='image',image={asset=string.rep('9',32),alt='Original synthetic palette',width=128,height=64}}};feature.anchors=J.array()
deliver7(feature);click('GET search form');click('q:');edit('new value');click('Submit GET');on_update();assert(endpoint('open').url=='https://example.org/search?q=new%20value&lang=en')
on_button('back',true);draw();cancelServer()
assert(not modules['model.lua'],'Assisted forms must not load Direct HTML parser')
click('Image');on_update();assert(endpoint('image').asset==string.rep('9',32));deliver7({v=1,error='too_large',retryable=false},413);assert(bodyText:find('Original synthetic palette',1,true)and bodyText:find('too_large',1,true),'image failure must retain alt plus per-image error');click('Image');on_update();endpoint('image');local r=request;request=nil;local pngPath=assert(os.getenv('ASSISTED_PNG_FIXTURE'));local png=test_read(pngPath,0,test_size(pngPath));resources[r.id+1000]=png;on_http{id=r.id,status=200,handle=r.id+1000,size=#png,content_type='image/png'};draw();assert(not text:find('Invalid assisted PNG',1,true))
local persisted=false;for path in pairs(files)do if path:match('b7%-%d+%.png$')then persisted=true end end;assert(persisted,'PNG must only persist after decode success')
on_exit();dofile('main.lua');on_init();draw();assert(text:find('Original forms',1,true));local retained=false;for path in pairs(files)do if path:match('b7%-%d+%.png$')then retained=true end end;assert(retained)
-- A legal 6KiB chunk can span over 512 narrow landscape screens. Find/reflow
-- walk only this chunk, retain the actual local page number, and history keeps
-- one semantic position rather than retaining every screen anchor.
-- Footerless reading adds rows: use large-font runs to retain the independent
-- >512-local-pages stress contract with the same legal 6KiB chunk.
w,h=800,400;draw();menu('Refresh page');on_update();endpoint('open');local dense=ready(0);dense.doc=string.rep('7',32);dense.chunks=1;dense.blocks=J.array{{id=1,kind='p',runs=J.array{{text=string.rep('x\n',1000)},{text=string.rep('y\n',1000)},{text=string.rep('z\n',1000)},{text='Original dense tail'}}}};dense.anchors=J.array{{id='dense-tail',block=1,run=4,byte=1}}
for _,run in ipairs(dense.blocks[1].runs)do run.size=2 end
deliver7(dense);menu('Find on page');edit('dense tail');on_update();endpoint('find');deliver7({v=1,found=true,chunk=0,block=1,run=4,byte=1});assert(text:find('Original dense tail',1,true),text);local densePage=tonumber(text:match('P(%d+) C1/1'));assert(densePage and densePage>512,'dense chunk must reach its actual local page beyond 512')
menu('Refresh page');on_update();endpoint('open');local other=ready(0);other.doc=string.rep('8',32);other.chunks=1;deliver7(other);click('<');assert(text:find('Original dense tail',1,true));on_button('page_back',true);draw();assert(text:find('P',1,true));w,h=480,800;draw()
-- Bounded history stores positions and cache references, never all chunk models.
for n=1,26 do menu('Refresh page');on_update();endpoint('open');local t=ready(0);t.doc=string.format('%032x',n);t.url='https://example.org/'..n;t.chunks=1;deliver7(t)end
menu('History');local seen={}
for page=1,10 do
 draw();local more
 for _,button in ipairs(modules['ui.lua'].buttons)do if button[6]:sub(1,8)=='https://'then seen[button[6]]=true elseif button[6]:sub(1,5)=='More 'then more=button[6]end end
 if not more or more:match('More (%d+)/')==more:match('/(%d+)$')then break end
 click('More')
end
local n=0;for _ in pairs(seen)do n=n+1 end;assert(n==24 and seen['https://example.org/26']and not seen['https://example.org/1'],'history24 bounded: '..n)
on_button('back',true);draw()
-- A missing history entry's explicit Retry must not overwrite a different
-- entry after new Direct navigation shifts the 24-entry history window.
menu('History');click('https://example.org/3');assert(not request)
menu('Mode:');click('Direct');menu('Address');edit('https://example.org/new-direct');on_update();assert(request.q.url=='https://example.org/new-direct');deliver('<article><h1>Original Direct fixture</h1><p>Direct body</p></article>')
menu('Retry last navigation');on_update();assert(request.q.url=='https://example.org/3');deliver('<article><h1>Original retried fixture</h1><p>Retried body</p></article>')
menu('History');local hasRetried=false
for page=1,10 do draw();local more;for _,button in ipairs(modules['ui.lua'].buttons)do if button[6]=='https://example.org/3'then hasRetried=true elseif button[6]:sub(1,5)=='More 'then more=button[6]end end;if not more or more:match('More (%d+)/')==more:match('/(%d+)$')then break end;click('More')end
assert(hasRetried,'stale Retry index overwrote another history entry');on_button('back',true);draw();menu('Mode:');click('Assisted');menu('Refresh page');on_update();endpoint('open');deliver7(ready(0))
-- Layout failure occurs before the B7 checked root/view transaction.
local old1,old2=files['saved/b7-index1'],files['saved/b7-index2'];local page=modules['layout.lua'].page
modules['layout.lua'].page=function(doc,...)if doc.srp and doc.srp.doc==string.rep('e',32)then error('synthetic layout failure')end;return page(doc,...)end
menu('Refresh page');on_update();endpoint('open');local broken=ready(0);broken.doc=string.rep('e',32);deliver7(broken)
modules['layout.lua'].page=page;assert(files['saved/b7-index1']==old1 and files['saved/b7-index2']==old2,'layout failure published a root');cancelServer();assert(files['saved/b7-index1']==old1 and files['saved/b7-index2']==old2,'cancel acknowledgement published a root');assert(text:find('Original opening',1,true))
-- Failed SD publication preserves view and marks cache uncertain.
menu('Refresh page');on_update();endpoint('open');fail=true;t=ready(0);t.doc=string.rep('e',32);deliver7(t);fail=false;assert(text:find('Original opening',1,true));assert(text:find('Assisted save failed',1,true))
menu('Refresh page');on_update();endpoint('open');failRoot=true;deliver7(t);failRoot=false;cancelServer();assert(text:find('Original opening',1,true));assert(text:find('SD cache read-only',1,true),text)
-- Existing B5/B1 namespaces and approved icon are untouched.
files['saved/page1.cache']='B1:legacy';on_exit();assert(files['saved/page1.cache']=='B1:legacy')
for _ in pairs(resources)do error('resource leaked')end
-- Old API7 keyed firmware is not public-capable, despite its old feature flag.
on_exit();smudge.has_feature=function(name)return name~='browser_backend_public_v1'end
dofile('main.lua');on_init();draw();assert(not modules['backend.lua'].available())
click('...')
for n=1,10 do
 local found=false;for _,b in ipairs(modules['ui.lua'].buttons)do assert(not b[6]:find('Import server',1,true));if b[6]:sub(1,5)=='Mode:'then found=true end end
 if found then break end;click('More')
end
click('Mode:');click('Assisted');on_button('back',true);draw()
assert(text:find('Update firmware',1,true),text);assert(not request)
on_exit()
print('PASS assisted input/navigation/poll/chunks/find/stale/cancel/SD/offline/reentry/font geometry; synthetic fixture')
