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

dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/heavy');on_update()
local r=assert(request);request=nil
local markup='<html><head><script>'..string.rep('let x="<ignored>";',25000)..'</script></head><body><article><p>NEVER PUBLISH CANCELLED</p></article></body></html>'
resources[1000]=markup
local reads,bytes=0,0;local original=smudge.http_read
smudge.http_read=function(handle,o,n)
 reads=reads+1;bytes=bytes+n
 -- Controlled host SD latency, not a physical Sticky measurement.
 local untilTime=os.clock()+0.0002;while os.clock()<untilTime do end
 return original(handle,o,n)
end
local checksum=modules['cache.lua'].sum(markup)
local before=os.clock();on_http{id=r.id,status=200,handle=1000,size=#markup,hash=checksum,url=r.q.url}
local elapsed=os.clock()-before
assert(elapsed<0.030,'HTTP callback monopolized UI '..elapsed..' seconds; Back/Home cannot be scheduled')
on_button('back',true);draw()
assert(text:find('Cancelled',1,true),'Back did not cancel parser work')
for i=1,500 do on_update()end;draw()
assert(not bodyText:find('NEVER PUBLISH',1,true),'cancelled document published')
assert(next(resources)==nil,'cancelled resource leaked')
on_exit();print('PASS real callback returns before parse; Back discards owned work',elapsed,reads,bytes)
-- Exercise shipped work over distinct long inner loops, not a fake work model.
dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/old');on_update()
deliver('<article><h1>STABLE OLD DOCUMENT</h1><p>Old body stays readable.</p></article>')
local function indexBytes()return (files['saved/b5-index1']or '')..(files['saved/b5-index2']or '')end
local stable=indexBytes();local maximum=0
for _,case in ipairs({'tag','script','css','css-comment','feed'})do
 click('...');click('Address');edit('https://example.org/'..case);on_update();local r=assert(request);request=nil
 local value
 if case=='tag'then value='<div data-large="'..string.rep('x',700000)..'"><p>CANDIDATE_TAG</p></div>'
 elseif case=='script'then value='<script>'..string.rep('let x="<tag>";',50000)..'</script><p>CANDIDATE_SCRIPT</p>'
 elseif case=='css'then
  local css='';for i=1,128 do css=css..'.c'..i..' .child {font-weight:bold;display:block}'end
  value='<style>'..css..'</style><article>'..string.rep('<p class="child">CANDIDATE_CSS</p>',300)..'</article>'
 elseif case=='css-comment'then value='<style>'..string.rep('/*x',8000)..'</style><p>CANDIDATE_MALFORMED_CSS</p>'
 else value='<rss><channel><title>Candidate feed</title>'..string.rep('<item><title>CANDIDATE_FEED</title><link>https://example.org/item</link></item>',100)..'</channel></rss>'end
 local handle=2000;resources[handle]=value
 on_http{id=r.id,status=200,handle=handle,size=#value,hash=modules['cache.lua'].sum(value),url=r.q.url}
 for _=1,2 do local start=os.clock();on_update();maximum=math.max(maximum,os.clock()-start)end
 local start=os.clock();on_button('back',true);maximum=math.max(maximum,os.clock()-start);draw()
 assert(indexBytes()==stable,'cancelled '..case..' changed checked index')
 assert(bodyText:find('STABLE OLD DOCUMENT',1,true),'cancelled '..case..' lost old page')
 assert(next(resources)==nil,'cancelled '..case..' leaked source')
 collectgarbage('collect')
end
-- A cooperatively held copy is cancelled before publication; native cursor
-- cleanup/readback/error/alias behavior is separately tested by resource.cpp.
click('...');click('Address');edit('https://example.org/copy');on_update();local r=assert(request);request=nil
resources[3000]='<p>COPY CANDIDATE</p>';local realStore=smudge.http_store_step;local copying=false
smudge.http_store_step=function()copying=true;return false end
on_http{id=r.id,status=200,handle=3000,size=#resources[3000],hash=modules['cache.lua'].sum(resources[3000]),url=r.q.url}
for i=1,1000 do on_update();if copying then break end end;assert(copying)
on_button('back',true);smudge.http_store_step=realStore;draw();assert(indexBytes()==stable and next(resources)==nil)
-- Native read error must be recoverable, with candidate and handle discarded.
click('...');click('Address');edit('https://example.org/read-error');on_update();r=assert(request);request=nil
resources[4000]='<p>READ ERROR CANDIDATE</p>';smudge.http_read=function()return nil,'storage_read'end
on_http{id=r.id,status=200,handle=4000,size=#resources[4000],hash=1,url=r.q.url};draw();smudge.http_read=original
assert(indexBytes()==stable and next(resources)==nil);on_exit()
assert(maximum<0.030,'work callback quantum '..maximum)
print('PASS long tag/script/CSS/feed, held cache copy, read error; actual browser jobs; max host CPU quantum',maximum)
-- A new fetch must not compete with a cancelled codec that still owns memory/SD.
dofile('main.lua');on_init();draw()
local heldImage=true;smudge.browser_image_busy=function()return heldImage end
click('...');click('Address');edit('https://example.org/after-image');on_update();assert(not request,'fetch started before cancelled codec drained')
heldImage=false;on_update();assert(request and request.q.url=='https://example.org/after-image')
on_button('back',true);smudge.browser_image_busy=nil;on_exit()
print('PASS new navigation waits for cancelled image owner to drain')

local words={};for i=1,6000 do words[#words+1]='word'..i..' 'end
local long='<p>FIRST_VISIBLE_VIEW '..table.concat(words)..' LAST_NEEDLE</p>';words=nil
local c=modules['cache.lua'].open();assert(c:save('https://example.org/long',long))
dofile('main.lua');on_init();draw();assert(bodyText:find('FIRST_VISIBLE_VIEW',1,true))
local function menu(label)
 click('...')
 for i=1,8 do
  draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then on_tap(b[1]+1,b[2]+1);draw();return end end
  click('More')
 end;error('menu missing '..label)
end
menu('Find on page');modules['ui.lua'].editor.value='LAST_NEEDLE';on_draw()
local advance=smudge.browser_advance or smudge.text_width
smudge.browser_advance=function(...)
 local untilTime=os.clock()+0.00005;while os.clock()<untilTime do end
 return advance(...)
end
for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Done'then on_tap(b[1]+1,b[2]+1);break end end
local reached=false
for i=1,1000 do on_update();on_draw();if modules['work.lua'].job and modules['work.lua'].job.stage=='Preparing page'then reached=true;break end end
assert(reached);on_update();on_button('back',true);on_draw();on_update()
assert(not modules['work.lua'].busy(),'cancelled Find reflow revived itself')
assert(bodyText:find('FIRST_VISIBLE_VIEW',1,true),'cancelled Find lost prior readable view')
smudge.browser_advance=advance;on_exit();print('PASS cancelled real Find reflow keeps prior page and does not revive')
-- Startup/offline checksum is a real yielded job, with checked cache unchanged.
local offlineIndex=indexBytes();dofile('main.lua');local started=os.clock();on_init();assert(os.clock()-started<0.030)
on_update();assert(modules['work.lua'].busy());on_button('back',true);on_draw();on_update()
assert(not modules['work.lua'].busy()and indexBytes()==offlineIndex,'offline cancellation revived or changed cache');on_exit()
dofile('main.lua');on_init();draw();assert(bodyText:find('FIRST_VISIBLE_VIEW',1,true))
local function locateMenu(label)
 click('...')
 for i=1,8 do
  draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then return b end end
  click('More')
 end;error('menu missing '..label)
end
local fontButton=locateMenu('Text size:');local oldFontLabel=fontButton[6]
on_tap(fontButton[1]+1,fontButton[2]+1);on_update();on_draw();on_update()
assert(modules['work.lua'].busy());on_button('back',true);on_draw();on_update()
assert(not modules['work.lua'].busy()and bodyText:find('FIRST_VISIBLE_VIEW',1,true),'font cancel lost readable view')
assert(locateMenu('Text size:')[6]==oldFontLabel,'font cancel did not restore font');on_button('back',true);draw()
-- Cancel in the middle of actual orientation pagination from a late Find hit.
menu('Find on page');edit('LAST_NEEDLE');assert(bodyText:find('LAST_NEEDLE',1,true))
local retainedWord=assert(bodyText:match('word%d+'));local retainedPage=assert(text:match('Page %d+'))
smudge.browser_advance=function(...)local untilTime=os.clock()+0.00005;while os.clock()<untilTime do end;return advance(...)end
w,h=800,480;on_draw();on_update();on_update();assert(modules['work.lua'].busy())
on_button('back',true);local steps=0;local idle=0
while idle<3 do steps=steps+1;on_draw();on_update();if modules['work.lua'].busy()then idle=0 else idle=idle+1 end;assert(steps<20,'cancelled rotation restarted whole-book reflow')end
on_draw();assert(bodyText:match('word%d+')==retainedWord and text:find(retainedPage,1,true),'rotation cancel lost retained visible anchor')
smudge.browser_advance=advance;on_exit()
print('PASS real offline startup/checksum, font rollback, middle rotation cancellation; retained visible anchor; recovery steps',steps)

dofile('main.lua');on_init();draw();click('...');click('Address');edit('https://example.org/image-page');on_update()
deliver('<article><img src="/picture" alt="Old image"><p>OLD IMAGE PAGE</p></article>')
click('...');click('Address');edit('https://example.org/held-fetch');on_update();assert(request)
on_draw();local tapped=false
for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Cancel'then on_tap(b[1]+b[3]//2,b[2]+b[4]//2);tapped=true;break end end
assert(tapped);draw();assert(not request,'Cancel overlay activated underlying image/link instead')
assert(text:find('Cancelled',1,true),'Cancel was lost to HTTP completion/action scheduling')
on_exit();print('PASS real Cancel overlay outranks retained image/link hits')
