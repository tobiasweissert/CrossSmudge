local host=dofile
APP=os.getenv('APP');assert(APP)
local modules={};MODULES=modules
function dofile(path)
 assert(path:match('^[a-z]+%.lua$'))
 if os.getenv('TRACE')then print('load',path,collectgarbage('count'))end
 local value=host('apps/'..APP..'/'..path);if path~='editor.lua' and path~='json.lua' and path~='digits.lua' and path~='forecast.lua'then modules[path]=value end;return value
end
local w,h=tonumber(os.getenv('WIDTH'))or 480,tonumber(os.getenv('HEIGHT'))or 800
WIDTH,HEIGHT=w,h
FILES={};TEXT='';NOW=0;DAY=15;FAIL=false;REQUEST=nil;CANCELLED=nil;EXITED=false;AWAKE=false
local function geometry(x,y,rw,rh)
 assert(x>=0 and y>=0 and rw>0 and rh>0 and x+rw<=w+1 and y+rh<=h+1,table.concat({'bounds',x,y,rw,rh,w,h},','))
end
local capture
local function emit(kind,...)
 if not capture then return end
 capture:write(kind)
 for _,v in ipairs({...})do capture:write('\t',(tostring(v):gsub('[\t\r\n]',' ')))end
 capture:write('\n');capture:flush()
end
local function width(s,font)return #s*(font=='small'and 7 or font=='bitter14'and 11 or 9)end
smudge={
 get_bounds=function()return w,h end,
 get_metrics=function()return{top_padding=10,header_height=40,button_hints_height=40,content_side_padding=20}end,
 get_date=function()return{year=2026,month=11,day=DAY,hour=12,min=0,wday=1}end,
 millis=function()return NOW end,
 read_file=function(p,n,offset)return FILES[p]and FILES[p]:sub((offset or 0)+1,(offset or 0)+n)end,
 write_file=function(p,s)if FAIL then FILES[p]=s:sub(1,10);return false end;FILES[p]=s;return true end,
 request_update=function()end,full_refresh=function()end,log=function()end,
 clear=function()
  TEXT='';if os.getenv('CAPTURE')then if capture then capture:close()end;capture=assert(io.open(os.getenv('CAPTURE'),'w'))end
 end,
 header=function(title,subtitle)
  TEXT=title..'\n'..(subtitle or '')..'\n';emit('text',20,15,title,'ui12','bold','left',true)
  emit('text',w-20,20,subtitle or '','small','regular','right',true);emit('line',0,54,w,54,1,true)
 end,
 button_hints=function()end,
 text_width=width,line_height=function(font)return font=='small'and 18 or font=='bitter14'and 30 or 26 end,
 text=function(x,y,s,font,style,align,color)
  assert(x>=0 and x<=w and y>=0 and y+smudge.line_height(font)<=h,'text bounds: '..x..','..y..' '..s)
  TEXT=TEXT..s..'\n';emit('text',x,y,s,font or 'ui12',style or 'regular',align or 'left',color~=false)
 end,
 wrapped_text=function(text,maxWidth,font,style,maxLines)
  local lines={};local line='';maxLines=maxLines or 100
  for word in text:gmatch('%S+')do
   if line~=''and width(line..' '..word,font)>maxWidth then lines[#lines+1]=line;line=word else line=line==''and word or line..' '..word end
   if #lines>=maxLines then break end
  end
  if #lines<maxLines and line~=''then lines[#lines+1]=line end
  return{lines=lines,total_height=#lines*smudge.line_height(font)}
 end,
 rounded_rect=function(x,y,rw,rh,r,filled,stroke,color)geometry(x,y,rw,rh);emit('rect',x,y,rw,rh,r,filled or false,stroke or 1,color~=false)end,
 rect=function(x,y,rw,rh,filled,stroke,color)geometry(x,y,rw,rh);emit('rect',x,y,rw,rh,0,filled or false,stroke or 1,color~=false)end,
 rect_dither=function(x,y,rw,rh)geometry(x,y,rw,rh);emit('dither',x,y,rw,rh)end,
 line=function(x,y,xx,yy,stroke,color)assert(x>=0 and y>=0 and xx<=w+1 and yy<=h+1,'line bounds');emit('line',x,y,xx,yy,stroke or 1,color~=false)end,
 circle=function(x,y,r,filled,stroke,color)geometry(x-r,y-r,2*r,2*r);emit('circle',x,y,r,filled or false,stroke or 1,color~=false)end,
 in_rect=function(x,y,rx,ry,rw,rh)return x>=rx and y>=ry and x<rx+rw and y<ry+rh end,
 prevent_sleep=function(v)AWAKE=v end,exit=function()EXITED=true end,
 has_feature=function()return not UNAVAILABLE end,
 http_request=function(options)
  local path=assert(options.url:match('^https://[^/]+(/.*)$'));assert(#options.url<=512 and #path<=384,'URL budget: '..#path)
  assert(not REQUEST);REQUEST={id=1,options=options};return 1
 end,
 http_cancel=function(id)CANCELLED=id;REQUEST=nil;return true end,
}
function draw()on_draw();assert(#TEXT<8000)end
function click(label)
 if os.getenv('TRACE')then print('click',label,collectgarbage('count'))end
 draw();local U=modules['ui.lua'];local x,y
 for _,b in ipairs(U.buttons)do if b[6]==label or b[6]:sub(1,#label)==label then x=b[1]+b[3]/2;y=b[2]+b[4]/2;break end end
 assert(x,'button missing: '..label..'\n'..TEXT)
 on_tap(x,y);collectgarbage('collect');draw()
end
function enter(text)draw();local U=modules['ui.lua'];assert(U.editor);U.editor.value=text;click('Done')end
function fixture(name)local f=assert(io.open('test/productivity/fixtures/'..name));local s=f:read('*a');f:close();return s end
function deliver(body,error,status)
 local request=assert(REQUEST);REQUEST=nil;on_http{id=request.id,body=body,error=error,status=status or 200};draw()
end
function restart()
 if on_exit then on_exit()end
 on_init=nil;on_draw=nil;on_button=nil;on_tap=nil;on_swipe=nil;on_update=nil;on_exit=nil;on_http=nil
 modules={};MODULES=modules;collectgarbage('collect');dofile('main.lua');collectgarbage('collect');on_init();draw()
end
local testCase=assert(loadfile('test/productivity/'..APP..'.lua'))
function RUN_CASE()
 restart()
 if os.getenv('PREVIEW')then
  if APP=='flashcards'then on_button('right',true);on_button('right',true)
  elseif APP=='habits'then on_button('confirm',true);on_button('down',true);on_button('down',true);on_button('confirm',true)
  elseif APP=='pomodoro'then on_button('confirm',true);NOW=420000;on_update()
  elseif APP=='weather'then click('Refresh');on_update();deliver(fixture('forecast.csv'))
  elseif APP=='browser'then click('Enter address');enter('example.com');on_update();deliver(fixture('page.txt'))end
  if os.getenv('PREVIEW')=='editor'then
   if APP=='flashcards'then click('+')elseif APP=='weather'then click('Location')elseif APP=='browser'then click('example.com')end
  end
  draw()
 else testCase()end
 print('PASS '..APP..' '..w..'x'..h)
end
