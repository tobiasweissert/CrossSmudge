-- Entry/navigation/save smoke for unchanged stock apps. UI text/font metrics
-- and persistence are explicit host fixtures; this is NOT a pixel parity gate.
local w,h=smudge.get_bounds();local saved={};local originals={};local clips={};local draws=0
local memory=smudge.get_memory
smudge.get_metrics=function()return{top_padding=12,header_height=50,button_hints_height=0,content_side_padding=20}end
smudge.line_height=function(font)return font=='small'and 20 or 28 end
smudge.is_button_down=function()return false end
smudge.get_date=function()return{year=2026,month=11,day=17,hour=12,min=0,sec=0,wday=3}end
smudge.load=function(k,default)return saved[k]or default end
smudge.save=function(k,v)saved[k]=v;return true end
smudge.wrapped_text=function(text,width,font,style,limit)
 local result={};local chars=math.max(1,width//10)
 for line in(text..'\n'):gmatch('(.-)\n')do
  if line==''then result[#result+1]=''end
  while #line>0 and #result<(limit or 100)do result[#result+1]=line:sub(1,chars);line=line:sub(chars+1)end
 end
 result.lines=result;result.total_height=#result*smudge.line_height(font);return result
end
smudge.find_section=function(path,tag,ending,maxOrCallback,callback)
 if type(ending)=='function'then callback=ending;ending='['end
 if type(maxOrCallback)=='function'then callback=maxOrCallback end
 local okay,data=pcall(smudge.read_file,path,1024*1024);if not okay or not data then return callback and false or nil end
 local lines={};local found=not tag or tag==''
 for line in(data..'\n'):gmatch('(.-)\n')do
  if found then
   if ending and ending~=''and line:sub(1,#ending)==ending then break end
   if callback then callback(line)else lines[#lines+1]=line end
  elseif line==tag then found=true end
 end
 return callback and found or table.concat(lines,'\n')
end
local function box(kind,x,y,bw,bh)
 draws=draws+1
 assert(x==x and y==y and bw==bw and bh==bh,'nonfinite '..kind)
 if x<0 or y<0 or x+bw>w or y+bh>h then clips[#clips+1]=table.concat({kind,x,y,bw,bh},',')end
end
for _,name in ipairs({'rect','rounded_rect','rect_dither','bitmap'})do
 local fn=smudge[name];originals[name]=fn
 smudge[name]=function(x,y,bw,bh,...)box(name,x,y,bw,bh);return fn(x,y,bw,bh,...)end
end
smudge.pixel=function(x,y)box('pixel',x,y,1,1)end
smudge.draw_sprite=function(x,y,bw,bh,data,...)box('sprite',x,y,bw,bh)end
smudge.heart=function(x,y,size)box('heart',x-size//2,y-size//2,size,size)end
smudge.thick_line=smudge.line
local function draw()if on_draw then on_draw()end end
local function button(key)if on_button then on_button(key,true);on_button(key,false);draw()end end
local app=dofile('main.lua');if on_init then on_init()end;draw()
for _,key in ipairs({'down','right','up','left','confirm','back'})do button(key)end
if on_update then on_update();draw()end
if on_tap then on_tap(w//2,h//2);draw()elseif on_touch then on_touch(w//2,h//2,'tap');draw()end
if on_exit then on_exit()end
print('STOCK_GEOMETRY',draws,#clips,table.concat(clips,'|'))
print('PASS actual stock entry/draw/button/touch/update/exit with host file/time/font boundaries')
