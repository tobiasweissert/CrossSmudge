-- Footerless contract via production app and the existing controlled adapter.
local rootHost=dofile
local p='test/browser/integration.lua';local chunks={}
for o=0,test_size(p)-1,8192 do chunks[#chunks+1]=test_read(p,o,math.min(8192,test_size(p)-o))end
local s=table.concat(chunks);local at=assert(s:find('local article=',1,true))
assert(load(s:sub(1,at-1)..[=[
local function counter(n)assert(text:find('Page '..n,1,true),'expected page '..n..': '..text)end
for _,size in ipairs({{480,800},{800,480}})do
 w,h=size[1],size[2];files={};request=nil
 dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/navigation');on_update()
 deliver('<article><h1>FIRST_MARKER</h1><p>'..string.rep('Readable numbered paragraphs. ',350)..'</p><p>LAST_MARKER</p></article>')
 local U=modules['ui.lua'];local c=U.content(0)
 for _,b in ipairs(U.buttons)do assert(b[6]~='Previous'and b[6]~='Next'and not b[6]:match('^Page '),'bottom pager hitbox retained')end
 assert(U.bottom==h-smudge.line_height('small')-16,'reading area still reserves button footer')
 assert(c.height==U.bottom-c.y,'prepare/draw geometry diverges')
 counter(1);local initial=text
 on_tap(w//2,h-14);draw();counter(1);assert(text==initial,'indicator tap changed reader')
 on_swipe('down');draw();counter(1)
 for _,key in ipairs({'page_forward','down','right'})do on_button(key,true);draw();counter(2);on_button('page_back',true);draw();counter(1)end
 for _,dir in ipairs({'up','left'})do on_swipe(dir);on_draw();counter(1);draw();counter(2);on_swipe('right');draw();counter(1)end
 on_button('page_forward',false);draw();counter(1)
 on_swipe('up');on_swipe('up');on_swipe('up');draw();counter(2)
 for _,key in ipairs({'up','left'})do on_button(key,true);draw();counter(1);on_swipe('up');draw();counter(2)end
 on_button('confirm',true);draw();assert(text:find('Document details',1,true));on_swipe('up');on_button('page_forward',true);draw();assert(text:find('Document details',1,true));on_button('back',true);draw();counter(2)
 local address=U.buttons[2];on_tap(address[1]+5,address[2]+5);draw();local ed=U.editor;assert(ed);on_swipe('left');assert(U.editor==ed);on_button('page_forward',true);assert(U.editor==ed);on_button('back',true);draw();counter(2)
 for _=1,100 do if text:find(' • end',1,true)then break end;on_button('page_forward',true);draw()end
 assert(text:find('LAST_MARKER',1,true));local last=text;on_swipe('up');draw();assert(text==last,'end bounds changed viewport')
 on_exit()
end
print('PASS footerless navigation: absent hitboxes, fitted indicator, expanded shared geometry, keys/swipes, fresh retained paint, coalescing, scopes, endpoints, both orientations')
]=],'reading-navigation'))()
-- Independently boot the existing SRP1 adapter; no alternate app model.
dofile=rootHost
local p='test/browser_assisted/client.lua';local chunks={}
for o=0,test_size(p)-1,8192 do chunks[#chunks+1]=test_read(p,o,math.min(8192,test_size(p)-o))end
local s=table.concat(chunks);local at=assert(s:find("dofile('main.lua');on_init()",1,true))
assert(load(s:sub(1,at-1)..[=[
for _,size in ipairs({{480,800},{800,480}})do
 for _,action in ipairs({'key','swipe'})do
  files={};request=nil;w,h=size[1],size[2]
  dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/held-image');on_update();endpoint('open')
  local t=ready(0);t.chunks=1;t.anchors=J.array()
  t.blocks=J.array{{id=1,kind='h1',runs=J.array{{text='IMAGE_FIRST'}}},{id=2,kind='image',image={asset=string.rep('9',32),alt='HELD_IMAGE',width=640,height=360}},{id=3,kind='p',runs=J.array{{text=string.rep('BODY_NEXT readable words. ',70)}}},{id=4,kind='p',runs=J.array{{text='IMAGE_END'}}}}
  deliver7(t);endpoint('image');local held=request;on_draw()
  assert(text:find('P1 C1/1',1,true)and text:find('IMAGE_FIRST',1,true));local first=text
  if action=='key'then on_button('page_forward',true)else on_swipe('up')end
  -- Every immediate/intermediate frame must retain coherent article content.
  for i=1,300 do
   on_draw();assert(not text:find('Loading - previous page retained',1,true),'held image paging blanked article')
   assert(text:find('IMAGE_FIRST',1,true)or text:find('BODY_NEXT',1,true),'article erased during held-image paging')
   if text:find('P2 C1/1',1,true)then break end
   on_update()
  end
  assert(text:find('P2 C1/1',1,true),'held image blocked '..action);assert(text~=first)
  assert(not request or request.id~=held.id,'held image did not cancel for '..action)
  resources[9999]='late image';on_http{id=held.id,status=200,handle=9999,size=10,content_type='image/png'}
  assert(not resources[9999],'stale image response survived paging')
  if action=='key'then on_button('page_back',true)else on_swipe('down')end
  draw();assert(text:find('P1 C1/1',1,true)and text:find('IMAGE_FIRST',1,true),'reverse paging lost first viewport')
  for _,b in ipairs(modules['ui.lua'].buttons)do assert(b[6]~='Next'and b[6]~='Previous'and not b[6]:match('^P%d+ C'),'stale bottom controls')end
  on_exit()
 end
end
print('PASS held automatic image: fresh coherent paints, physical mapped key callback and swipe callback, cancellation, late body release, reverse page, both orientations (callback coverage; native HAL separately)')
]=],'reading-navigation-assisted'))()
