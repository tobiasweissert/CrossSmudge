-- Parent A12: inspect strings actually passed to the real font painter.
local pixelRead=smudge.read_file

local function glyphPixels(path,glyphs)
 local w,h=smudge.get_bounds();local header='P5\n'..w..' '..h..'\n255\n';local checked=0
 -- Test-only full frame read: host storage adapter does not implement offsets.
 local frame=assert(pixelRead(path,#header+w*h));assert(#frame==#header+w*h)
 for _,b in ipairs(glyphs)do for row=0,b.h-1 do
  local pixels=frame:sub(#header+(b.y+row)*w+1,#header+(b.y+row+1)*w)
  for col=0,b.w-1 do if (b.data:byte(row*((b.w+7)//8)+col//8+1)>>(7-col%8))&1==0 then
   assert(pixels:byte(b.x+col+1)==(b.paper and 255 or 0),'production glyph ink erased/overpainted '..(b.x+col)..','..(b.y+row)..' got '..tostring(pixels:byte(b.x+col+1))..' paper '..tostring(b.paper));checked=checked+1
  end end
 end end
 assert(checked>0,'actual glyph pixels required')
end

local load=dofile;local directions={};local glyphs={};local measuring=false
local clear=smudge.clear;smudge.clear=function(...)directions={};glyphs={};return clear(...)end
local bitmap=smudge.bitmap
smudge.bitmap=function(x,y,w,h,data,paper,...)
 if measuring then glyphs[#glyphs+1]={x=x,y=y,w=w,h=h,data=data,paper=paper}end
 return bitmap(x,y,w,h,data,paper,...)
end
dofile=function(path)
 local module=load(path)
 if path=='sticky_graphics.lua'then local text=module.text;module.text=function(value,...)
  if value:find('^LEFT T')or value:find('^TOP T')then directions[#directions+1]=value;measuring=true end
  local result=text(value,...);measuring=false;return result
 end end
 return module
end
local function completeDirections(path)
 assert(#directions>=2,'direction glyphs missing')
 for _,value in ipairs(directions)do assert(value=='LEFT TO RIGHT'or value=='TOP TO BOTTOM','A12-HEX-LANDSCAPE-DIRECTIONS: '..value)end
 if path then glyphPixels(path,glyphs)end;directions={};glyphs={}
end
local read=smudge.read_file;local saves={};smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)saves[path]=v;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();on_button('confirm',true);on_draw();on_button('back',true);on_draw();on_button('confirm',true);on_draw()
local function shot(name)
 on_draw();capture(name..'.pgm');local g=app.state();local s={};for i=1,121 do s[i]=g.b[i]end;assert(fixtureWrite(name..'.map',table.concat(s)..'\n'..table.concat({g.turn,g.status,app.preferences.lastMove,2,app.preferences.seat},' ')..'\n',false))
end
local function tap(c,r)local l=app.layout();on_touch(l.x+2*l.a+3*l.a*c,l.y+l.half+2*l.half*r+l.half*c,'tap')end
on_draw();capture('directions-turn1.pgm');completeDirections('directions-turn1.pgm')
for outcome=1,2 do
 if outcome==1 then for row=0,10 do tap(0,row);if row<10 then on_draw();if row==0 then capture('directions-turn2.pgm');completeDirections('directions-turn2.pgm')else completeDirections()end;tap(10,row)end end
 else for col=0,10 do tap(col==10 and 1 or 0,col==10 and 10 or col+1);tap(col,0)end end
 assert(app.state().status==outcome);shot('result-'..outcome);assert(app.preferences.wins+app.preferences.losses==outcome)
 on_button('confirm',true);on_draw();assert(app.state().status==0,'physical result Confirm did not replay')
end
-- Actual solo callbacks in both seat roles and both turns: direction ink stays complete.
for _,seat in ipairs({2,1})do
 on_button('back',true);on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='settings')
 if app.preferences.nextMode~=1 then on_button('confirm',true);on_draw()end
 on_button('down',true);on_button('down',true)
 if app.preferences.nextSeat~=seat then on_button('confirm',true);on_draw()end
 on_button('back',true);on_draw();on_button('confirm',true);on_draw();assert(app.preferences.seat==seat and app.state().turn==1)
 local name='directions-seat'..seat..'-turn1.pgm';capture(name);completeDirections(name)
 if seat==2 then local guard=0;while app.state().turn==1 do on_update();guard=guard+1;assert(guard<1000)end
 else tap(0,0)end
 on_draw();assert(app.state().turn==2);name='directions-seat'..seat..'-turn2.pgm';capture(name);completeDirections(name)
end
on_exit()
