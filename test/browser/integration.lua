local host=dofile;local modules={}
function dofile(n)local value=host('apps/browser/'..n);modules[n]=value;return value end
local resources={};local files={};local text='';local bodyText='';local request;local id=0;local fail=false;local w,h=480,800
smudge={api_version=test_api_version or 5,has_feature=function()return true end,request_update=function()end,
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
local article='<html><head><title>My article</title></head><body><article><h1 id="top">Useful story</h1><p>'..string.rep('Words for pagination. ',250)..'<a href="/two">Next article</a></p><img src="/photo" alt="Photo"></article></body></html>'
dofile('main.lua');on_init();draw();click('Address');edit('https://example.org/one');on_update();assert(request.q.url=='https://example.org/one');deliver(article);assert(text:find('Useful story',1,true));capture('article-portrait')
click('Next');assert(text:find('Page 2',1,true))
click('...');click('Address');edit('https://example.org/two');on_update();deliver('<article><h1>Second story</h1><p>Another useful body of text with enough content.</p></article>');assert(text:find('Second story',1,true))
click('<');assert(text:find('Page 2',1,true)and not text:find('Second story',1,true));click('Previous');assert(text:find('Useful story',1,true));click('Next') -- truthful history pagination
click('...');click('Forward website');assert(text:find('Second story',1,true))
click('...');click('Address');edit('https://example.org/fail');on_update();local old=assert(request);on_button('back',true);on_http{id=old.id,status=200,body='<p>STALE</p>'};draw();assert(text:find('Second story',1,true)and not text:find('STALE',1,true))
click('...');click('Address');edit('https://example.org/diskfail');on_update();fail=true;deliver('<p>Should never replace current</p>');fail=false;assert(text:find('Second story',1,true))
on_exit();dofile('main.lua');on_init();draw();assert(text:find('Second story',1,true))
-- portrait/landscape same callbacks/geometry; keyboard controls remain bounded.
w,h=800,480;draw();capture('article-landscape');click('...');draw();capture('menu-landscape');click('Address');draw();capture('keyboard-landscape');on_button('back',true);draw()
-- GET forms in simplified mode (production model and encoder).
local M=modules['model.lua'];local doc=assert(M.parse('<form action="/s"><input name="q" value="a &amp; b"><input type="hidden" name="lang" value="en"></form>','https://example.org/',{reader=false}));assert(M.formURL(doc.forms[1])=='https://example.org/s?q=a%20%26%20b&lang=en')
-- Transactional cache failure and old namespace preservation.
files['page1.cache']='B1:legacy';local C=modules['cache.lua'];local c=C.open();assert(c:read(c.entries[1]));local previous=c.entries[1].file;fail=true;assert(not c:save('https://example.org/fail','new'));fail=false;assert(c.entries[1].file==previous and files['page1.cache']=='B1:legacy')
-- Busy cancellation drains without losing queued navigation.
w,h=480,800;local realRequest=smudge.http_request;local busy=true
smudge.http_request=function(q)if busy then return nil,'busy'end;return realRequest(q)end
click('...');click('Address');edit('example.net');on_update();assert(not request);busy=false
on_update();assert(request.q.url=='https://example.net/');smudge.http_request=realRequest
-- External CSS survives checked offline reopen.
deliver('<head><style>h1 {font-weight:bold}</style><link rel="stylesheet" href="/sheet.css"></head><article><h1>Styled page</h1><p class="hide">Keep hidden</p><a href="/follow">Follow article</a><img src="/photo" alt="Photo"></article>')
assert(request and request.q.url=='https://example.net/sheet.css');deliver('.hide {display:none!important}')
assert(not text:find('Keep hidden',1,true));on_exit();dofile('main.lua');on_init();draw();assert(not text:find('Keep hidden',1,true))
-- Inline link hit follows its actual target.
click('Follow article');on_update();assert(request.q.url=='https://example.net/follow');deliver('<article><h1>Followed link</h1><p>Useful destination</p></article>')
-- Bookmark controls and offline list are reachable through paginated menus.
local function menuItem(label)
 click('...')
 for _=1,8 do
  draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then click(label);return end end
  click('More')
 end;error('menu item '..label)
end
menuItem('Bookmark this page');assert(text:find('Bookmark saved',1,true));on_button('back',true);draw()
menuItem('Bookmarks');assert(text:find('example.net/follow',1,true));on_button('back',true);draw()
-- Mode transition from a later simplified block cannot leave a blank reader.
menuItem('Simplified page mode');draw();assert(text:find('Followed link',1,true))
-- Cache cleanup keeps objects referenced by either checked slot.
for i=1,12 do assert(c:save('https://example.org/cache/'..i,'<p>'..i..'</p>'));c:cleanup()end
local count=0;for k in pairs(files)do if k:match('b5%-%d+%.page$')then count=count+1 end end;assert(count<=16,'unbounded page objects')
click('...');click('Address');edit('https://example.org/adjacent');on_update();deliver('<p>Start <a href="/a">A</a><a href="/b">B</a> end</p>')
draw();local A,B;for _,button in ipairs(modules['ui.lua'].buttons)do if button[6]=='A'then A=button elseif button[6]=='B'then B=button end end
assert(A and B and A[1]+A[3]<=B[1],'link hits overlap');on_tap(B[1]+1,B[2]+1);draw();on_update();assert(request.q.url=='https://example.org/b');on_button('back',true)
w,h=800,480;click('...');click('Address');local ed=modules['ui.lua'].editor;ed.value='https://example.org/'..string.rep('abcdefghi ',40)..'TAIL';ed.cursor=#ed.value+1;draw();assert(text:find('TAIL',1,true)and text:find('|',1,true),'cursor viewport lost tail');capture('keyboard-long-landscape');on_button('back',true)
w,h=480,800;click('...');click('Address');edit('https://example.org/mixed');on_update();deliver('<article><h1>Browser field test</h1><p>Original synthetic fixture • 15 November 2026</p><p>Read <strong>bold emphasis</strong> and <a href="/details">an inline link</a> with reserved image geometry.</p><img src="/photo" alt="Original test gradient"><ul><li>First useful item</li><li>Second useful item</li></ul><pre>  code stays indented</pre><table><tr><th>Name</th><th>Value</th></tr><tr><td>Bound</td><td>1024 KiB</td></tr></table><p style="border:1px solid black;background:#eee">Bounded CSS border and background.</p></article>')
if font_image_data then
 draw();click('Image');on_update();deliver(test_read('test/browser/fixtures/gradient.png',0,test_size('test/browser/fixtures/gradient.png')));capture('mixed-portrait-image');click('Image');capture('image-zoom-portrait');on_button('back',true);draw();click('Next');capture('mixed-portrait-blocks')
end
if os.getenv('BROWSER_LIVE_BODY')then
 local path=os.getenv('BROWSER_LIVE_BODY');local size=test_size(path);local hash=1
 for offset=0,size-1,8192 do hash=modules['cache.lua'].sum(test_read(path,offset,math.min(8192,size-offset)),hash)end
 click('...');click('Address');edit('https://news.example.org/live-capture');on_update();deliver({path=path,size=size,hash=hash});if request then deliver('')end
 capture('real-article-portrait');w,h=800,480;draw();capture('real-article-landscape')
end
draw();local retainedAddress=modules['ui.lua'].buttons[2][6]
click('...');click('Address');edit('https://example.org/resource-error');on_update();deliver('<link rel="stylesheet" href="/broken.css"><p>Candidate must stay unpublished until CSS processing completes.</p>')
local originalRead=smudge.http_read;smudge.http_read=function()return nil,'storage_read'end
local successful,err=pcall(deliver,'p {display:none}');smudge.http_read=originalRead
assert(successful,err);draw();assert(modules['ui.lua'].buttons[2][6]==retainedAddress,'failed resource replaced page')
-- Cached and network history must restore the mode together with its anchors.
w,h=480,800;menuItem('Reader mode');click('...');click('Address');edit('https://example.org/history-c');on_update();deliver('<article><p>Reader C stable position and useful text for history.</p></article>')
local historyHTML='<nav><p>'..string.rep('NAVIGATION ',900)..'</p></nav><article><p>Actual short article text.</p></article>'
click('...');click('Address');edit('https://example.org/history-a');on_update();deliver(historyHTML);menuItem('Simplified page mode');click('Next');click('Next');click('Next');assert(text:find('Page 4',1,true))
click('<');assert(text:find('Reader C stable',1,true));menuItem('Forward website');assert(text:find('Page 4',1,true)and text:find('NAVIGATION',1,true),'cached history lost mode/position');click('Previous');assert(text:find('Page 3',1,true));click('Next')
click('<');local index=modules['cache.lua'].open();for _,e in ipairs(index.entries)do if e.url=='https://example.org/history-a'then files['saved/'..e.file]=nil end end
menuItem('Forward website');on_update();assert(request and text:find('Reader C stable',1,true),'failed history candidate changed current view');deliver(historyHTML);assert(text:find('Page 4',1,true)and text:find('NAVIGATION',1,true),'network history lost mode/position')

-- Final repair regressions use only production UI callbacks and captured draws.
local repair=os.getenv('BROWSER_REPAIR_CASE')
if not repair or repair=='find'then
 click('...');click('Address');edit('https://example.org/late-find');on_update()
 deliver('<p>'..string.rep('start café words ',100)..'NEEDLE_AT_END tail</p>')
 assert(not bodyText:find('NEEDLE_AT_END',1,true))
 menuItem('Find on page');edit('NEEDLE_AT_END')
 assert(bodyText:find('NEEDLE_AT_END',1,true),'Find did not display late span match')
 local doc=assert(M.parse('<p>café Ω tail</p>','https://example.org/'))
 local anchor=modules['layout.lua'].find(doc,'Ω');assert(anchor.p==7 and utf8.len(doc.blocks[1].spans[1].text:sub(anchor.p)))
end
if not repair or repair=='fragment'then
 click('...');click('Address');edit('https://example.org/fragments');on_update()
 deliver('<meta name="author" content="Author"><nav><p>Filtered navigation</p></nav><article><p><a href="#target">Jump target</a> <a href="#wrapper">Jump wrapper</a></p><p>'..string.rep('Filler words ',140)..'</p><section id="wrapper"><a id="empty"></a><h2 id="target"><strong>Nested heading</strong></h2><p>'..string.rep('Destination body. ',10)..'</p></section></article>')
 click('Jump target');assert(not text:find('Fragment not found',1,true)and text:find('Nested heading',1,true),'nested heading fragment lost')
 menuItem('Start of document');click('Jump wrapper');assert(text:find('Nested heading',1,true),'wrapper fragment lost')
 local parsed=assert(M.parse('<meta name="author" content="A"><nav><p>DROP</p></nav><article><div id="wrap"><a id="empty"/><h2 id="nested"><em>Heading</em></h2><p>'..string.rep('body ',30)..'</p></div></article>','https://example.org/'))
 for _,id in ipairs({'wrap','empty','nested'})do local a=assert(parsed.anchors[id],id);assert(parsed.blocks[a.b].kind=='h2'and a.s==1 and a.p==1,'reader anchor reindex')end
 local many='';for i=1,300 do many=many..'<a id="id'..i..'"></a>'end
 parsed=assert(M.parse(many..'<p>Text</p>','https://example.org/'));local count=0;for _ in pairs(parsed.anchors)do count=count+1 end;assert(count<=256,'unbounded IDs')
end
if not repair or repair=='history'then
 -- Reset to an isolated offline entry; repeated opens must not retain >24 views.
 on_exit();files={};dofile('main.lua');on_init();draw()
 click('Address');edit('https://example.org/bounded');on_update()
 local body='<p>'..string.rep('Remember this pagination anchor. ',150)..'</p>';deliver(body)
 for i=1,30 do click('Next');assert(text:find('Page 2',1,true));menuItem('Offline pages');click('https://example.org/bounded')end
 for i=1,23 do click('<');assert(text:find('History',1,true)and text:find('Page 2',1,true),'retained offline anchor lost '..text)end
 click('<');assert(text:find('No previous website',1,true),'offline history exceeded 24 entries')
 -- Network push after back must prune forward entries; mixing paths shares cap.
 for i=1,30 do
  if i%2==0 then
   click('...');click('Address');edit('https://example.org/bounded');on_update();deliver(body)
  else menuItem('Offline pages');click('https://example.org/bounded')end
 end
 for i=1,23 do click('<');on_update();if request then deliver(body)end;assert(text:find('History',1,true)or text:find('Direct HTTPS',1,true),'mixed history '..i..' '..text)end
 click('<');assert(text:find('No previous website',1,true),'mixed history exceeded 24 entries')
end
on_exit();assert(next(resources)==nil,'resource leaked after exit')
print('PASS app callbacks navigation/anchors/cancel/stale/failed SD/reopen/forms/touch geometry')
