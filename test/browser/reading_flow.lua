-- Reuse the existing controlled storage/transport adapter, not its scenarios.
local parts={};local path='test/browser/integration.lua'
for offset=0,test_size(path)-1,8192 do parts[#parts+1]=test_read(path,offset,math.min(8192,test_size(path)-offset))end
local source=table.concat(parts)
local prefix=assert(source:find('local article=',1,true))
local cases=[=[
-- RF-1: ordinary callback scheduling, without layout overrides or forced yields.
local function textSizeAction()
 click('...')
 for _=1,8 do
  for _,b in ipairs(modules['ui.lua'].buttons)do
   if b[6]:find('Text size:',1,true)then
    -- Rapid duplicate contacts before the action runs must not compound reflows.
    on_tap(b[1]+5,b[2]+5);on_tap(b[1]+5,b[2]+5);return
   end
  end
  click('More')
 end
 error('text-size action unavailable')
end
local function rf1()
 for _,bounds in ipairs({{480,800},{800,480}})do
  w,h=bounds[1],bounds[2];files={}
  dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/reflow');on_update()
  deliver('<article><h1>RETAIN_MARKER</h1><p><a href="#top">RETAIN_LINK</a></p><p id="top">'..string.rep('Readable paragraphs words. ',200)..'</p></article>')
  local label='reflow-'..w
  local function visible()
   on_draw();assert(text:find('RETAIN_MARKER',1,true),'RF-1 text-size reflow erased retained article: '..text)
  end
  local function linkGeometry()
   for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='RETAIN_LINK'then return b[1]..':'..b[2]..':'..b[3]..':'..b[4]end end
  end
  visible();local old=assert(linkGeometry());capture(label..'-before')
  textSizeAction();on_update();visible();capture(label..'-during')
  assert(linkGeometry()==old,'retained hit geometry changed before publication')
  -- Pager retaps coalesce while the replacement is queued/running.
  on_swipe('up');on_swipe('up');on_update();visible()
  assert(modules['work.lua'].busy(),'replacement not pending at cancellation boundary')
  on_button('back',true);visible();assert(linkGeometry()==old,'cancel failed to restore old geometry');draw();visible()
  capture(label..'-cancelled')
  textSizeAction();on_update();visible()
  for i=1,20000 do on_update();visible();if not modules['work.lua'].busy()then
   -- draw() also drains the ordinary queued layout request.
   draw();break
  end end
  visible();capture(label..'-after')
  assert(linkGeometry()~=old,'replacement font geometry was not adopted')
  -- Retained fragment hits must resolve against the same document.
  click('RETAIN_LINK');draw();assert(text:find('Readable',1,true),'retained fragment target lost')
  click('...');click('Address');edit('https://example.org/replacement');on_update()
  deliver('<article><h1>NEW_DOCUMENT</h1><p>Replacement content only.</p></article>')
  assert(text:find('NEW_DOCUMENT',1,true)and not text:find('RETAIN_MARKER',1,true),'old document lease survived new document adoption')
  on_exit()
 end
 print('PASS RF-1 ordinary text-size frames / old hits / cancel / pager retaps / replacement / both orientations')
end
rf1();files={};w,h=480,800

local layouts=0
local function frame(marker)
 on_draw()
 assert(not text:find('Loading - previous page retained',1,true),'same-document work hid article: '..text)
 assert(text:find(marker,1,true),'readable viewport lost: '..text)
 for _,b in ipairs(modules['ui.lua'].buttons)do assert(b[6]~='Next'and b[6]~='Previous','obsolete pager hitbox')end
 assert(text:find('Page ',1,true),'same-document work removed progress')
end
local function boot()
 dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/flow');on_update()
 deliver('<article><h1>FLOW_START</h1><p>'..string.rep('Readable paragraph words. ',200)..'</p><p>FLOW_END</p></article>')
 local L=modules['layout.lua'];local page=L.page
 L.page=function(...)layouts=layouts+1;modules['work.lua'].check();return page(...)end
end
if os.getenv('BROWSER_FLOW_CASE')~='image'then
boot()
local old=text
on_swipe('up');on_draw()
assert(text:find('FLOW_START',1,true),'page turn blanked before preparation: '..text)
on_update();frame('FLOW_START')
-- Repeated gestures during preparation coalesce, never skip unread pages.
on_swipe('up');on_swipe('up')
for i=1,1000 do on_update();on_draw();if not modules['work.lua'].busy()and text:find('Page 2',1,true)then break end end
assert(text:find('Page 2',1,true),'page turn did not publish page 2')
assert(layouts==1,'avoidable page layout: '..layouts)
on_swipe('down');on_update();on_button('back',true);on_draw()
assert(text:find('Page 2',1,true),'cancel replaced visible page')
local count=0;local L=modules['layout.lua'];local real=L.page;local seen={};local duplicate
L.page=function(doc,a,width,height,font)
 local key=width..':'..height..':'..a.b..':'..a.s..':'..a.p
 if seen[key]then duplicate=key end;seen[key]=true;count=count+1
 return real(doc,a,width,height,font)
end
w,h=800,480;draw();assert(count>0,'rotation did not prepare layout');assert(not duplicate,'duplicate reflow: '..tostring(duplicate));L.page=real
print('PASS coherent page publication / one layout / repeated paging / cancel / bounded rotation')
else boot()end
-- A small article reserves image geometry; network/decode must stay inline.
click('...');click('Address');edit('https://example.org/image');on_update()
deliver('<article><h1>INLINE_ARTICLE</h1><p>Text remains readable.</p><img src="/photo" alt="Inline diagram"><p>AFTER_IMAGE</p></article>')
click('Image');on_update();frame('INLINE_ARTICLE')
local first=assert(request).id
click('Image');on_update();frame('INLINE_ARTICLE')
assert(request.id==first,'repeated tap restarted image request')
local poll=0
smudge.browser_image_poll=function()poll=poll+1;return poll<5 and 0 or 1 end
smudge.browser_image_blit=function(path)return files[path]~=nil end
local r=request;request=nil;local handle=r.id+1000
resources[handle]='\137PNG\r\n\26\ncontrolled bytes'
on_http{id=r.id,status=200,handle=handle,size=#resources[handle],url=r.q.url}
for i=1,100 do on_update();frame('INLINE_ARTICLE');if poll>=5 and not modules['work.lua'].busy()then break end end
assert(poll>=5,'decode not completed');assert(next(resources)==nil,'image response leaked')
print('PASS inline fetch/store/decode and repeated image tap')
-- A failed request/decode leaves an inline retry, not a stuck loading label.
click('...');click('Address');edit('https://example.org/fail-image');on_update()
deliver('<article><h1>ERROR_ARTICLE</h1><img src="/bad" alt="Failure diagram"></article>')
click('Image');on_update();local failed=request;request=nil
on_http{id=failed.id,status=503};draw();frame('ERROR_ARTICLE')
assert(text:find('Image fetch failed',1,true),'image failure not shown inline: '..text)
click('Image');on_update();local stale=assert(request);on_button('back',true);frame('ERROR_ARTICLE')
resources[9999]='stale';on_http{id=stale.id,status=200,handle=9999,size=5};draw();assert(resources[9999]==nil,'stale response leaked')
click('Image');on_update()
smudge.browser_image_start=function()return false,'decode_start_failure'end
deliver('\137PNG\r\n\26\nfixture');draw();frame('ERROR_ARTICLE')
assert(text:find('decode_start_failure',1,true),'decode failure missing')
-- Retry must not be blocked by stale preparingImage, nor open zoom before decode.
smudge.browser_image_start=function(source,output)files[output]=files[source];return true end
smudge.browser_image_poll=function()return -1 end
click('Image');draw();frame('ERROR_ARTICLE')
assert(text:find('Image decode failed',1,true),'failed decoder cannot retry inline')
assert(not text:find('Image downloaded',1,true),'decode failure still advertised as downloaded')
smudge.browser_image_poll=function()return 0 end
for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Image'then on_tap(b[1]+5,b[2]+5);break end end
on_draw();on_update();on_update();on_draw();assert(modules['work.lua'].busy())
-- New address cancels a held decoder through the real header action.
local addressButton=modules['ui.lua'].buttons[2];on_tap(addressButton[1]+5,addressButton[2]+5);draw()
assert(modules['ui.lua'].editor,'address unreachable during decode');edit('https://example.org/replacement');on_update()
deliver('<h1>REPLACEMENT_ARTICLE</h1><p>New URL wins.</p>');assert(text:find('REPLACEMENT_ARTICLE',1,true))
-- Home/exit rejects a late HTTP body and releases it immediately.
on_exit();resources[8888]='late';on_http{id=99999,status=200,handle=8888,size=4};assert(next(resources)==nil)
print('PASS inline failure/retry/cancel, new URL during decode, exit/stale resources')
]=]
assert(load(source:sub(1,prefix-1)..cases,'reading-flow'))()
