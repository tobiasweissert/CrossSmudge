-- Production app callbacks, controlled SRP1 transport/storage (not live service).
local path='test/browser_assisted/client.lua';local parts={}
for offset=0,test_size(path)-1,8192 do parts[#parts+1]=test_read(path,offset,math.min(8192,test_size(path)-offset))end
local source=table.concat(parts);local at=assert(source:find("dofile('main.lua');on_init()",1,true))
assert(load(source:sub(1,at-1)..[=[
local clear=smudge.clear;smudge.clear=function()bodyText='';clear()end
local function feature(doc)
 local t=ready(0);t.doc=doc or string.rep('f',32);t.chunks=1;t.anchors=J.array()
 t.blocks=J.array{{id=1,kind='p',runs=J.array{{text='Text must be painted first.'}}},{id=2,kind='image',image={asset=string.rep('9',32),alt='Visible photograph',width=640,height=360}},{id=3,kind='p',runs=J.array{{text=string.rep('Below the fold. ',90)}}},{id=4,kind='image',image={asset=string.rep('8',32),alt='Offscreen photograph',width=640,height=360}}}
 return t
end
local function start()
 dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/one');on_update();endpoint('open')
end
start()
-- Finish production navigation without a paint, then eagerly update again.
local r=request;request=nil;local s=J.encode(feature());resources[r.id+1000]=s
on_http{id=r.id,status=200,handle=r.id+1000,size=#s,content_type='application/json'}
for _=1,2000 do on_update()end
assert(not request,'automatic request preceded first completed text paint')
on_draw();assert(text:find('Text must be painted first.',1,true));assert(text:find('Visible photograph',1,true));assert(not request,'network started inside paint')
on_update();local q=endpoint('image');assert(q.asset==string.rep('9',32),'only visible asset eligible');assert(q.width==320 and q.height==180)
local first=request.id;for _=1,20 do on_draw();on_update()end;assert(request.id==first,'one operation in flight')
deliver7({v=1,error='too_large',retryable=false},413)
for _=1,30 do on_draw();on_update()end;assert(not request,'automatic failure retried');assert(text:find('Visible photograph',1,true))
-- Explicit retry remains reachable; Back invalidates stale completion.
click('Image');on_update();endpoint('image');local stale=request;on_button('back',true);draw();cancelServer()
resources[999]='invalid';on_http{id=stale.id,status=200,handle=999,size=7,content_type='image/png'};draw();assert(not resources[999]and not request)
-- Opt-out is exercised through the real menu, not an internal test switch.
menu('Images: Automatic');assert(not request);menu('Refresh page');on_update();endpoint('open');deliver7(feature(string.rep('e',32)))
assert(not request,'tap-to-load preference was lost on navigation');assert(text:find('Visible photograph',1,true))
menu('Images: Tap-to-load');on_update();endpoint('image')
on_button('back',true);draw();cancelServer();menu('Mode:');click('Offline');for _=1,20 do on_draw();on_update()end;assert(not request,'Offline implicit network')
on_exit();assert(next(resources)==nil)
-- A valid worker PNG reaches checked B7 publication only after decode.
files={};request=nil;start();deliver7(feature(string.rep('d',32)));endpoint('image')
local pngPath=assert(os.getenv('ASSISTED_PNG_FIXTURE'));local png=test_read(pngPath,0,test_size(pngPath))
local function imageReply(bytes,size)
 local r=assert(request);request=nil;resources[r.id+1000]=bytes
 on_http{id=r.id,status=200,handle=r.id+1000,size=size or #bytes,content_type='image/png'};draw()
 assert(not resources[r.id+1000],'response handle leaked')
end
smudge.browser_image=function()return true end
imageReply(png)
assert(imageJob and imageJob.width==320 and imageJob.height==180,'decoder box differs from request')
assert(not text:find('Visible photograph',1,true),'decoded image did not replace placeholder')
local pngCount=0;for p in pairs(files)do if p:match('b7%-%d+%.png$')then pngCount=pngCount+1 end end;assert(pngCount==1)
menu('Refresh page');on_update();endpoint('open');deliver7(feature(string.rep('d',32)));assert(not request,'same-document refresh refetched cached asset');assert(not text:find('Visible photograph',1,true),'same-document refresh lost cached inline image')
menu('Mode:');click('Offline');draw();assert(not request,'cached Offline image fetched')
on_exit();request=nil;dofile('main.lua');on_init();draw();assert(not request,'warm B7 image fetched')
-- No auto retry for oversized or invalid PNG; explicit manual retry remains.
menu('Refresh page');on_update();endpoint('open');deliver7(feature(string.rep('b',32)));endpoint('image')
imageReply(string.rep('x',16385));assert(text:find('Automatic',1,true)and text:find('image limit',1,true),'oversize error hidden: '..text);assert(not request)
click('Image');on_update();endpoint('image');imageReply('not a PNG');assert(text:find('Invalid assisted PNG',1,true));assert(not request)
-- Six eligible images across viewports, but only four automatic attempts.
local function many(doc)
 local t=feature(doc);t.blocks=J.array()
 for n=1,6 do t.blocks[#t.blocks+1]={id=n,kind='image',image={asset=string.format('%032x',n),alt='Content image '..n,width=640,height=360}}end
 return t
end
menu('Refresh page');on_update();endpoint('open');deliver7(many(string.rep('6',32)))
local attempts=0
for _=1,16 do
 draw()
 if request then endpoint('image');attempts=attempts+1;deliver7({v=1,error='unavailable',retryable=true},503)
 else on_button('page_forward',true);draw()end
end
assert(attempts==4,'document automatic attempt limit: '..attempts)
-- Charge received bytes even when the PNG is invalid; reserve the next ceiling.
menu('Refresh page');on_update();endpoint('open');deliver7(many(string.rep('4',32)))
local charged=0
for _=1,16 do
 draw()
 if request then endpoint('image');charged=charged+1;imageReply(string.rep('x',16000))else on_button('page_forward',true);draw()end
end
assert(charged==3,'aggregate automatic byte limit: '..charged)
-- Canonical Assisted producer filters explicit positive tiny hints upstream;
-- wire 1 means unknown metadata, including missing or partial publisher sizes.
-- This policy does not infer natural size or apply to Direct transport.
for index,dims in ipairs{{1,1},{1,360},{640,1}}do
 menu('Refresh page');on_update();endpoint('open');local unknown=feature(string.format('%032x',700+index))
 unknown.blocks[2].image.width=dims[1];unknown.blocks[2].image.height=dims[2]
 local r=request;request=nil;local body=J.encode(unknown);resources[r.id+1000]=body
 on_http{id=r.id,status=200,handle=r.id+1000,size=#body,content_type='application/json'}
 for _=1,2000 do on_update()end
 assert(not request,'unknown metadata requested before first readable paint')
 on_draw();assert(text:find('Text must be painted first.',1,true));assert(not request,'request inside paint')
 on_update();assert(request,'unknown Assisted dimensions blocked automatic photograph');endpoint('image')
 imageReply(png);assert(not text:find('Visible photograph',1,true),'unknown photograph not decoded inline')
end
for _,dims in ipairs{{16,24},{24,360},{640,24}}do
 menu('Refresh page');on_update();endpoint('open');local tiny=feature(string.rep('3',32));tiny.blocks[2].image.width=dims[1];tiny.blocks[2].image.height=dims[2];deliver7(tiny);assert(not request,'known-small image auto fetched')
end
-- New document gets a fresh allowance; cancelled automatic work yields to paging.
menu('Refresh page');on_update();endpoint('open');deliver7(many(string.rep('5',32)));endpoint('image')
local cancelled=request.id;on_button('page_forward',true);draw();assert(not request or request.id~=cancelled,'foreground paging blocked by automatic fetch')
-- Menu and text reflow must never wait for an image response.
on_button('back',true);draw();cancelServer();menu('Refresh page');on_update();endpoint('open');deliver7(feature(string.rep('2',32)));endpoint('image')
local held=request;menu('Text size:');assert(not request,'font reflow waited for held image')
assert(text:find('Text must be painted first.',1,true),'font reflow blanked article')
resources[998]=png;on_http{id=held.id,status=200,handle=998,size=#png,content_type='image/png'};draw();assert(not resources[998],'late image survived foreground reflow')
-- Replacement document cannot adopt an old image completion.
menu('Refresh page');on_update();endpoint('open');deliver7(feature(string.rep('1',32)));endpoint('image');held=request
menu('Address');edit('https://example.org/replacement');on_update();endpoint('open');deliver7(ready(0))
resources[997]=png;on_http{id=held.id,status=200,handle=997,size=#png,content_type='image/png'};draw();assert(not resources[997]);assert(text:find('Original opening',1,true),'old image replaced new document')
-- Direct never routes image URLs through the backend, even with automatic enabled.
on_button('back',true);draw();cancelServer();menu('Mode:');click('Direct');menu('Address');edit('https://example.org/direct');on_update()
assert(request.q.url=='https://example.org/direct');deliver('<article><p>Direct text first.</p><img src="/photo.png" alt="Direct image"></article>')
for _=1,20 do on_draw();on_update()end;assert(not request,'Direct image fetched implicitly')
on_exit();assert(next(resources)==nil)
-- Physical Menu must invalidate both fetch and decode before late success.
local function savedPNGs()
 local n=0;for p in pairs(files)do if p:match('b7%-%d+%.png$')then n=n+1 end end;return n
end
local repairCase=os.getenv('AUTO_REPAIR_CASE')
for _,phase in ipairs{'fetch','decode'}do
 if not repairCase or repairCase==phase then
  files={};request=nil;imageJob=nil;start();deliver7(feature(string.rep(phase=='fetch'and 'c'or 'a',32)));endpoint('image')
  local held=assert(request);local decoderHeld=true
  local originalPoll=smudge.browser_image_poll
  if phase=='decode'then
   smudge.browser_image_poll=function()return decoderHeld and 0 or 1 end
   request=nil;resources[996]=png
   on_http{id=held.id,status=200,handle=996,size=#png,content_type='image/png'}
   for _=1,2000 do on_update();if imageJob then break end end
   assert(imageJob,'held decoder was not reached')
  end
  on_button('confirm',true)
  if phase=='fetch'then
   request=nil;resources[995]=png
   on_http{id=held.id,status=200,handle=995,size=#png,content_type='image/png'}
  else decoderHeld=false end
  draw()
  assert(savedPNGs()==0,'physical Menu allowed late '..phase..' B7 publication')
  assert(not resources[995]and not resources[996],'Menu leaked a response handle')
  assert(text:find('Document details',1,true),'physical Menu did not open')
  smudge.browser_image_poll=originalPoll
  -- Physical Menu does not cancel an explicit manual image fetch.
  on_button('back',true);draw();click('Image');on_update();endpoint('image')
  on_button('confirm',true);imageReply(png)
  assert(savedPNGs()==1,'physical Menu cancelled explicit manual loading')
  assert(text:find('Document details',1,true),'manual image completion closed Menu')
  on_exit();request=nil
 end
end
-- COMBINED-AIR-STORE-1: real response coroutine held at Resource::copyStep's
-- incomplete boundary (native initialization returns incomplete even for a tiny PNG).
-- Only the host I/O callback is held; ownership/action state is never forged.
if not repairCase or repairCase=='store'then
 files={};request=nil;imageJob=nil;start();deliver7(feature(string.rep('7',32)));endpoint('image')
 local originalStore,originalStart=smudge.http_store_step,smudge.browser_image_start
 local heldStore,entered,decodeStarts=true,false,0
 smudge.http_store_step=function(...)
  entered=true;if heldStore then return false end;return originalStore(...)
 end
 smudge.browser_image_start=function(...)decodeStarts=decodeStarts+1;return originalStart(...)end
 local function roots()
  local result={};for p,bytes in pairs(files)do if p:find('b7-index',1,true)then result[p]=bytes end end;return result
 end
 local before=roots()
 local r=request;request=nil;resources[992]=png
 on_http{id=r.id,status=200,handle=992,size=#png,content_type='image/png'}
 for _=1,2000 do on_update();if entered then break end end
 assert(entered and modules['work.lua'].busy()and resources[992],'incomplete store boundary not reached')
 on_button('confirm',true)
 heldStore=false;draw() -- remaining ordinary updates must not revive cancelled storage.
 assert(not resources[992],'physical Menu leaked held-store response handle')
 assert(decodeStarts==0,'physical Menu allowed held-store decode admission')
 assert(savedPNGs()==0,'physical Menu allowed held-store B7 publication')
 local after=roots();for p,bytes in pairs(before)do assert(after[p]==bytes,'Menu changed B7 root')end
 for p,bytes in pairs(after)do assert(before[p]==bytes,'Menu added B7 root')end
 for p in pairs(files)do assert(not p:find('b7-download',1,true),'Menu leaked temporary download')end
 assert(text:find('Document details',1,true),'held-store Menu did not open')
 on_button('back',true);draw()
 assert(bodyText:find('Text must be painted first.',1,true)and bodyText:find('P1 C1/1',1,true),'Menu return lost article/progress')
 for _,b in ipairs(modules['ui.lua'].buttons)do assert(b[6]~='Previous'and b[6]~='Next','phantom footer paging target')end
 -- Explicit manual retry is held at exactly the same I/O boundary: Menu must
 -- open without cancelling this unrelated operation, then normal settlement wins.
 click('Image');on_update();endpoint('image');r=request;request=nil;resources[991]=png
 heldStore=true;entered=false
 on_http{id=r.id,status=200,handle=991,size=#png,content_type='image/png'}
 for _=1,2000 do on_update();if entered then break end end
 assert(entered and resources[991],'manual store control not reached')
 on_button('confirm',true);assert(resources[991],'Menu cancelled explicit manual store')
 heldStore=false;draw()
 assert(not resources[991]and decodeStarts==1 and savedPNGs()==1,'manual held-store completion failed')
 assert(text:find('Document details',1,true),'manual held-store completion closed Menu')
 smudge.http_store_step=originalStore;smudge.browser_image_start=originalStart
 on_exit();assert(next(resources)==nil);request=nil
 print('PASS COMBINED-AIR-STORE-1: held incomplete copy, physical Menu, handle release, zero decode/publication, unchanged roots, coherent return; manual store unaffected')
end
-- The download budget is soft: reserve 16KiB before admission, then charge
-- every body, including rejected PNGs and JSON/errors. The native 64KiB/body
-- cap permits one overshoot (at most 32KiB prior + 64KiB final = 96KiB).
if not repairCase or repairCase=='budget'then
 local function response(bytes,meta)
  local r=assert(request);request=nil;local handle=r.id+1000;resources[handle]=bytes
  local e={id=r.id,status=meta.status or 200,handle=handle,size=#bytes,content_type=meta.content_type or 'image/png',error=meta.error}
  if meta.size~=nil then e.size=meta.size end
  if meta.missing then e.size=nil end
  on_http(e);on_http(e) -- duplicate notification cannot double-charge.
  draw();assert(not resources[handle],'budget response leaked')
 end
 local sequences={
  {sizes={65536,65536,65536,65536},expected=1},
  {sizes={16384,16384,65536,1},expected=3},
  {sizes={16000,16000,4096,1},expected=3,status=503,content_type='application/json'},
  {sizes={16000,16000,4096,1},expected=3,error='network'},
  {sizes={16000,16000,4096,1},expected=3,content_type='text/plain'},
  {sizes={1,1,1,1},expected=1,size=-1},
  {sizes={1,1,1,1},expected=1,size=2147483647},
  {sizes={1,1,1,1},expected=1,size=0/0},
  {sizes={1,1,1,1},expected=1,size=1.5},
  {sizes={1,1,1,1},expected=1,size='65536'},
  {sizes={1,1,1,1},expected=1,missing=true},
  {sizes={0,0,0,0},expected=4},
 }
 -- Deterministic bounded random sequences exercise the production callbacks,
 -- not just a parallel budget model. Keep arithmetic within the int32 VM.
 local seed=71
 for _=1,24 do
  local sizes={};for n=1,4 do seed=(seed*251+17)%65521;sizes[n]=seed+16 end
  sequences[#sequences+1]={sizes=sizes}
 end
 local maximum=0
 for index,seq in ipairs(sequences)do
  files={};request=nil;imageJob=nil;start();deliver7(many(string.format('%032x',index+100)))
  local expected,total=0,0
  for _,size in ipairs(seq.sizes)do if expected<4 and total+16384<=49152 then expected=expected+1;total=total+size else break end end
  if seq.expected then expected=seq.expected end
  local attempts,wire=0,0
  for _=1,20 do
   draw()
   if request then
    endpoint('image');attempts=attempts+1
    assert(attempts<=expected,'rejected response body escaped automatic budget, sequence '..index)
    local size=seq.sizes[attempts];wire=wire+size
    local meta=(seq.status or seq.error or seq.content_type)and attempts<=2 and {}or seq
    local bytes=string.rep('x',size)
    if meta.status then local json=J.encode({v=1,error='unavailable',retryable=true});bytes=json..string.rep(' ',size-#json)end
    response(bytes,meta)
   else on_button('page_forward',true);draw()end
  end
  assert(attempts==expected,'incorrect automatic response accounting, sequence '..index..': '..attempts..'/'..expected)
  assert(wire<=98304,'native-capped response sequence exceeded 96KiB')
  maximum=math.max(maximum,wire)
  -- Budget exhaustion must not remove explicit manual loading.
  on_button('page_back',true);draw();click('Image');on_update();endpoint('image');imageReply(png)
  assert(savedPNGs()==1,'manual retry failed after automatic stop')
  on_exit();assert(next(resources)==nil);request=nil;collectgarbage('collect')
 end
 assert(maximum==98304,'maximum one-body overshoot was not exercised')
 -- Stale generations cannot charge the replacement document's allowance.
 files={};request=nil;start();deliver7(many(string.rep('e',32)));endpoint('image');local old=request
 menu('Address');edit('https://example.org/new-budget');on_update();endpoint('open');deliver7(many(string.rep('d',32)));endpoint('image')
 local current=request;resources[994]=string.rep('x',65536)
 on_http{id=old.id,status=503,handle=994,size=65536,content_type='application/json'}
 assert(not resources[994]and request==current,'stale response changed current image request')
 local fresh=0
 for _=1,20 do draw();if request then endpoint('image');fresh=fresh+1;response(string.rep('x',16000),{})else on_button('page_forward',true);draw()end end
 assert(fresh==3,'stale generation depleted the replacement document budget')
 on_exit();request=nil
 -- Already-received bytes remain charged if Menu cancels the queued handoff.
 files={};start();deliver7(many(string.rep('c',32)));endpoint('image')
 local queued=request;request=nil;resources[993]=string.rep('x',65536)
 on_http{id=queued.id,status=200,handle=993,size=65536,content_type='image/png'}
 on_button('confirm',true);draw();assert(not resources[993],'cancelled handoff leaked')
 on_button('back',true);draw()
 for _=1,12 do assert(not request,'Menu cancellation refunded a received image body');on_button('page_forward',true);draw()end
 on_exit();request=nil
 print('PASS automatic body budget: rejected/status/error, invalid sizes, duplicates, manual retry; 36 actual callback sequences, maximum 98304 bytes (soft 49152, native body cap 65536)')
end
print('PASS automatic visible Assisted images: text-first, bounded box, serialized, failure latch, explicit retry, stale cancel, session opt-out, Offline; PNG publication/warm-cache, invalid/oversize, four attempts, renew, foreground paging, Direct manual')
]=],'auto-images'))()
