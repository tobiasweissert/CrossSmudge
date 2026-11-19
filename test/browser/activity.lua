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
local function draw()on_draw()end
local function click(label)
 draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then on_tap(b[1]+b[3]//2,b[2]+b[4]//2);draw();return end end
 error('missing '..label..' in '..text)
end
local function edit(s)modules['ui.lua'].editor.value=s;click('Done')end
local function deliver(s,url)
 local r=assert(request);request=nil;local handle=r.id+1000;resources[handle]=s;local hash=type(s)=='table'and s.hash or modules['cache.lua'].sum(s);on_http{id=r.id,status=200,handle=handle,size=type(s)=='table'and s.size or #s,hash=hash,url=url or r.q.url};on_update();draw()
end

for key,value in pairs(http_api)do smudge[key]=value end
smudge.browser_path=function(name)return test_root..'/'..name end
local read=smudge.read_file
smudge.read_file=function(path,n,o)return read(path,n,o)or test_read(path,o or 0,n)end
function test_navigate(url)
 click('Address');edit(url)
end
function test_text()return text end
function test_idle()return not modules['work.lua'].busy()end
function test_stage()return modules['work.lua'].job and modules['work.lua'].job.stage or ''end
function test_address()on_draw();local b=modules['ui.lua'].buttons[2];on_tap(b[1]+1,b[2]+1)end
function test_edit(url)edit(url)end
function test_index()
 local result='';for i=1,2 do local path=test_root..'/b5-index'..i;result=result..(files[path]or '')end;return result
end

function test_cancel_point()on_draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Cancel'then return (b[1]+1)..':'..(b[2]+1)end end;error('Cancel missing')end

local listFiles,deleteFile=smudge.list_files,smudge.delete_file
smudge.list_files=function(path)local names=listFiles(path);for _,name in ipairs(test_fs_list(path))do names[#names+1]=name end;return names end
smudge.delete_file=function(path)deleteFile(path);return test_fs_remove(path)end

function test_menu_point()on_draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='...'then return (b[1]+1)..':'..(b[2]+1)end end;error('menu missing')end
function test_menu_open()for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]=='Address / search'then return 'yes'end end;return 'no'end
