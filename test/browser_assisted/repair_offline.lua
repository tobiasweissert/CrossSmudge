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
smudge.browser_backend_configured=function()return configured end
smudge.browser_backend_configure=function()imports=imports+1;configured=true;return true end
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

dofile('main.lua');on_init();draw();menu('Import server configuration')
click('Address');edit('https://example.org/one');on_update();endpoint('open')
local t=ready(0);t.chunks=1;t.blocks=J.array{{id=1,kind='p',runs=J.array{{text='hello, '},{text='world',bold=true}}}};t.anchors=J.array()
deliver7(t);menu('Mode:');click('Offline')
local function findLocal(q,found)
 menu('Find on page');edit(q);assert(not request,'Offline started network');assert(text:find(found and 'Found in saved chunks' or 'No match in saved chunks',1,true),text)
end
-- Read the real app's retained position through its normal online Find-next request.
local function position(q,run,byte)
 menu('Mode:');click('Assisted');menu('Find on page');edit(q);on_update()
 local a=endpoint('find').after;assert(a~=J.null and a.chunk==0 and a.block==1 and a.run==run and a.byte==byte,'actual UTF8/run position '..J.encode(a))
 -- Return exactly the admitted position, preserving Find-next state across the menu.
 deliver7({v=1,found=true,chunk=0,block=1,run=run,byte=byte});menu('Mode:');click('Offline')
end
findLocal('hello, world',true);position('hello, world',1,1)
findLocal('hello, world',false)
menu('Mode:');click('Assisted');menu('Refresh page');on_update();endpoint('open')
t=ready(0);t.doc=string.rep('b',32);t.chunks=1;t.blocks=J.array{{id=1,kind='p',runs=J.array{{text='hello, '},{text='world',bold=true},{text=' hello, world • é'},{text='té été',href='https://example.org/link'},{text=' • banana'}}},{id=2,kind='p',runs=J.array{{text='separate'}}},{id=3,kind='p',runs=J.array{{text='paragraph'}}}};t.anchors=J.array()
deliver7(t);menu('Mode:');click('Offline')
findLocal('HELLO, world',true);position('HELLO, world',1,1)
findLocal('HELLO, world',true);position('HELLO, world',3,2)
findLocal('HELLO, world',false)
findLocal('été',true);position('été',3,19)
findLocal('été',true);position('été',4,5)
findLocal('été',false);findLocal('Été',false)
findLocal('ana',true);position('ana',5,7)
findLocal('ana',true);position('ana',5,9)
findLocal('ana',false);findLocal('separateparagraph',false)
-- Missing chunks stay a partial search and never trigger Offline network requests.
menu('Mode:');click('Assisted');menu('Refresh page');on_update();endpoint('open')
t=ready(0);t.doc=string.rep('c',32);t.blocks=J.array{{id=1,kind='p',runs=J.array{{text='saved '},{text='piece',bold=true}}}};t.anchors=J.array()
deliver7(t);assert(text:find('1/2 saved',1,true),text);menu('Mode:');click('Offline')
findLocal('saved piece',true);position('saved piece',1,1);findLocal('missing tail',false)
assert(text:find('partial search',1,true));assert(not request);on_exit();print('PASS CB-LUA-002 real menu/tap/input Offline styled UTF8/exclusive overlapping Find-next')
