-- Actual Assisted jobs and B7 cache; controlled transport, no backend service.
local path='test/browser_assisted/client.lua';local parts={}
for offset=0,test_size(path)-1,8192 do parts[#parts+1]=test_read(path,offset,math.min(8192,test_size(path)-offset))end
local source=table.concat(parts);local at=assert(source:find("dofile('main.lua');on_init()",1,true))
assert(load(source:sub(1,at-1)..[=[
local clear=smudge.clear;smudge.clear=function()bodyText='';clear()end
dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/one');on_update();endpoint('open');deliver7(ready(0))
on_button('page_forward',true);draw();on_update();endpoint('chunk');on_draw()
assert(text:find('Original opening',1,true),'remote chunk fetch hid old article: '..text)
assert(not text:find('Loading - previous page retained',1,true))
local r=request;on_swipe('left');on_update();assert(request.id==r.id,'repeated chunk paging restarted request')
deliver7(ready(1));assert(text:find('Original ending',1,true))
on_button('page_back',true);draw();assert(text:find('Original opening',1,true))
-- Remote Find also keeps the current article during location lookup.
menu('Find on page');edit('ending');on_update();endpoint('find');on_draw()
assert(text:find('Original opening',1,true),'Find hid current article: '..text)
on_button('back',true);draw();cancelServer();assert(text:find('Original opening',1,true))
-- A cancelled stale completion cannot publish a new chunk or retain handles.
resources[999]='{}';on_http{id=r.id,status=200,handle=999,size=2};draw()
assert(not resources[999]);assert(text:find('Original opening',1,true))
on_exit();assert(next(resources)==nil)
print('PASS Assisted remote/cached chunk visibility, repeated paging, Find cancel/stale')
]=],'reading-flow-assisted'))()
