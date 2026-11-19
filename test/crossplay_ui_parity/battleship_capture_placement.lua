-- Parent A12: observe actual production glyph bitmap bounds, never substitute fonts.
local pixelRead=smudge.read_file

local function glyphPixels(path,glyphs)
 local w,h=smudge.get_bounds();local header='P5\n'..w..' '..h..'\n255\n';local checked=0
 -- Test-only full frame read: host storage adapter does not implement offsets.
 local frame=assert(pixelRead(path,#header+w*h));assert(#frame==#header+w*h)
 for _,b in ipairs(glyphs)do for row=0,b.h-1 do
  local pixels=frame:sub(#header+(b.y+row)*w+1,#header+(b.y+row+1)*w)
  for col=0,b.w-1 do if (b.data:byte(row*((b.w+7)//8)+col//8+1)>>(7-col%8))&1==0 then
   assert(pixels:byte(b.x+col+1)==(b.paper and 255 or 0),'production glyph ink erased/overpainted');checked=checked+1
  end end
 end end
 assert(checked>0,'actual glyph pixels required')
end

local load=dofile;local reportInk={};local measuring=false
local bitmap=smudge.bitmap
smudge.bitmap=function(x,y,w,h,data,paper,...)
 if measuring then reportInk[#reportInk+1]={x=x,y=y,w=w,h=h,data=data,paper=paper}end
 return bitmap(x,y,w,h,data,paper,...)
end
dofile=function(path)
 local module=load(path)
 if path=='graphics.lua'then local text=module.text;module.text=function(value,x,y,size,...)
  if size==10 and x==16 then reportInk={};measuring=true end
  local result=text(value,x,y,size,...);measuring=false;return result
 end end
 return module
end
local function readable(l)
 for _,b in ipairs(reportInk)do
  assert(b.y+b.h<=l.y-9 or b.y>=l.y+l.cell*10+9 or b.x+b.w<=l.x-9 or b.x>=l.x+l.cell*10+9,
   'A12-BATTLESHIP-LANDSCAPE-REPORT: production report ink overlaps playable grid')
 end
 assert(#reportInk>0,'report must render actual font ink')
end
local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:match('^state%-')then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function shot(name)
 on_draw();local p=app.layout().placement
 assert(write('battleship-place-'..name..'.fleet',p.board..'\n'..p.selected..'\n'..p.message..'\n',false))
 capture('battleship-place-'..name..'.pgm');readable(app.layout());glyphPixels('battleship-place-'..name..'.pgm',reportInk)
end
shot('fresh')
local l=app.layout();local bow=l.placement.board:find('1',1,true)
local x,y=l.x+(bow-1)%10*l.cell+l.cell//2,l.y+(bow-1)//10*l.cell+l.cell//2
on_touch(x,y,'tap');shot('selected');on_touch(x,y,'tap');shot('turned')
on_touch(l.rotate.x+l.rotate.w//2,l.rotate.y+l.rotate.h//2,'tap');shot('shuffled')
local ready=app.layout().view;on_touch(ready.x+ready.w//2,ready.y+ready.h//2,'tap');on_draw();capture('battleship-report-green.pgm');readable(app.layout());glyphPixels('battleship-report-green.pgm',reportInk)
on_exit()
