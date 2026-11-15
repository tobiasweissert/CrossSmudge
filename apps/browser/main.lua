local U,M,C=dofile('ui.lua'),dofile('model.lua'),dofile('cache.lua')
local url,doc,cache,pending,active,closed,menu,linkPage,history,offsets,page,nextOffset,cached,suspended
local status='Public web text via Jina; URLs sent to Jina'
local function redraw()U.redraw()end
local function open(target)
 target=M.url(target)
 if not target then status='Enter a public HTTPS address';redraw();return end
 if active or pending then status='Wait or Back to cancel';redraw();return end
 if doc and #doc.text>4096 then
  if not cached then status='Page not cached; close and retry later';redraw();return end
  suspended=true;doc=nil;U.buttons={};collectgarbage('collect')
 end
 pending=target;status='Queued';menu=false;redraw()
end
local function address()
 if active or pending then status='Wait or Back to cancel';return end
 if doc and #doc.text>4096 then
  if not cached then status='Page not cached; Refresh to retry saving';return end
  suspended=true;doc=nil;collectgarbage('collect')
 end
 U.edit('Web address',url or '',300,function(s)
  if not s:find('%.') and s:find('%S')then s='https://en.wikipedia.org/w/index.php?search='..U.encode(s)end
  open(s)
 end)
end
local function turn(delta)
 if not doc then return end
 if delta>0 and nextOffset and nextOffset<=#doc.text then offsets[page+1]=nextOffset;page=page+1
 elseif delta<0 and page>1 then page=page-1 end
 redraw()
end
function on_init()
 closed=false;history={};offsets={1};page=1;linkPage=1;menu=false
 cache=C.open('page',M.cache);collectgarbage('collect')
 if cache.body then local c=M.cache(cache.body);url=c.url;doc=M.parse(cache.body,c.start);cached=true;offsets={doc.start};status='Offline cache';cache.body=nil;collectgarbage('collect')end
 redraw()
end
function on_update()
 if closed then return end
 if suspended and not U.editor and not active and not pending then
  U.editorDraw=nil;collectgarbage('collect')
  local raw=cache:read();doc=raw and M.parse(raw);suspended=false
  if doc then offsets={doc.start};page=1 end
  if not doc then status='Cached page unavailable';offsets={1};page=1 end
  redraw()
 end
 if active or not pending then return end
 local target=pending;pending=nil
 if not smudge.has_feature('https_async')then status='HTTPS unavailable in simulator';redraw();return end
 local id,err=smudge.http_request{url='https://r.jina.ai/'..target,headers={Accept='text/plain'},timeout_ms=30000}
 if id then active={id=id,url=target};status='Loading public text...'
 else status='Request failed: '..tostring(err)end;redraw()
end
function on_http(e)
 if closed or not active or e.id~=active.id then return end
 local target=active.url;active=nil
 if e.error or e.status~=200 then
  status=e.error=='body_limit'and 'Page exceeds 8 KB; previous page kept'
   or e.error=='time_untrusted'and 'Trust verified UTC in App Settings'
   or ('Fetch failed: '..tostring(e.error or e.status));redraw();return
 end
 if not M.valid(e.body)then status='Invalid text response; previous page kept';redraw();return end
 doc=nil;U.buttons={};collectgarbage('collect')
 local parsed=M.parse(e.body)
 if url and url~=target then table.insert(history,1,url);if #history>6 then table.remove(history)end end
 url=target;doc=parsed;suspended=false;offsets={parsed.start};page=1;linkPage=1;collectgarbage('collect')
 local saved=cache:save(e.body,url..'\n');cached=saved
 status=saved and 'Public text via Jina Reader'or 'Loaded; cache save failed'
 U.focus=1;redraw()
end
function on_draw()
 if U.editor then U.editDraw();return end
 U.begin('Browser',status)
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 local host=url and url:match('^https://([^/]+)')or 'Enter address'
 U.button(host,p,t,w-2*p-58,42,address)
 U.button('...',w-p-48,t,48,42,function()menu=not menu;U.focus=1 end)
 local y=t+62
 if menu then
  local actions={{'Enter address / search',address},{'Refresh page',function()if url then open(url)end end},
   {'Previous website',function()if #history>0 then local u=table.remove(history,1);open(u)end end}}
  for i,a in ipairs(actions)do U.button(a[1],p,y+(i-1)*46,w-2*p,40,a[2])end
  U.text(p,y+184,'Links: first 8, within 1 KB','small')
  local ly=y+210;local rows=math.max(1,(b-ly-65)//40);local links=doc and doc.links or {}
  local first=(linkPage-1)*rows+1
  for i=first,math.min(#links,first+rows-1)do local link=links[i]
   U.button('['..i..'] '..U.fit(link[1],w-2*p-45),p,ly+(i-first)*40,w-2*p,36,function()open(link[2])end)
  end
  if #links>rows then U.button('More links',p,b-56,w-2*p,36,function()linkPage=linkPage%math.ceil(#links/rows)+1 end)end
 else
  if doc then
   local rows=math.max(1,(b-y-90)//smudge.line_height('ui12'))
   local lines;lines,nextOffset=M.page(doc.text,offsets[page],w-2*p,rows)
   for i,s in ipairs(lines)do
    U.text(p,y+(i-1)*smudge.line_height('ui12'),s,'ui12')
    if s:find('%[%d+%]')then smudge.line(p,y+i*smudge.line_height('ui12')-2,p+smudge.text_width(s,'ui12'),y+i*smudge.line_height('ui12')-2)end
   end
   U.text(w/2,b-82,'Page '..page..(nextOffset>#doc.text and ' - end'or ''),'small',false,'center')
  else U.wrap('Read public web pages as text. Addresses are sent to Jina Reader. No logins, forms or scripts. Pages must fit 8 KB.',p,y,w-2*p,6)end
  local bw=(w-2*p-16)//3
  U.button('Previous',p,b-58,bw,40,function()turn(-1)end)
  U.button('Links',p+bw+8,b-58,bw,40,function()menu=true;U.focus=1 end)
  U.button('Next',p+2*(bw+8),b-58,bw,40,function()turn(1)end)
 end
 U.footer()
end
local function cancel()pending=nil;if active then smudge.http_cancel(active.id);active=nil end end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if key=='back'then U.closeEditor();redraw()else U.key(key)end;return end
 if key=='back'then
  if active or pending then cancel();status='Cancelled'
  elseif menu then menu=false else closed=true;smudge.exit()end
 elseif not menu and (key=='down' or key=='right' or key=='page_forward')then turn(1)
 elseif not menu and (key=='up' or key=='left' or key=='page_back')then turn(-1)
 elseif not menu and key=='confirm'then address()
 else U.key(key)end;redraw()
end
function on_tap(x,y)U.tap(x,y)end
function on_swipe(dir)if not U.editor and not menu then turn(dir=='left'and 1 or -1)end end
function on_exit()closed=true;cancel()end
