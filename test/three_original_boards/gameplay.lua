local read,write=smudge.read_file,smudge.write_file;local saves={}
smudge.read_file=function(path,cap,offset)
 if saves[path]then return saves[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:match('%.dat$')then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)if TEST_RECORD_FAILURE and path:match('^native%-record')then return false end;saves[path]=value;return true end
local M=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function shot(name)
 on_draw();capture(name..'.pgm');local g=app.state();local b={};for _,v in ipairs(g.b)do b[#b+1]=v end
 assert(write(name..'.map',table.concat(b)..'\n'..table.concat({g.turn,g.status,g.selected or 0,g.die or 0,g.idle or 0},' ')..'\n'..(g.history or '')..'\n',false))
end
local function tapCell(index)
 local l=app.layout();local x,y
 if M.cols==8 then x=l.x+(index-1)%8*l.cell+l.cell//2;y=l.y+(index-1)//8*l.cell+l.cell//2
 elseif M.cols==7 then x=l.x+(index-1)*l.cell+l.cell//2;y=l.y+l.lip//2
 else x=l.x+(index-1)*(l.cell+l.gap)+l.cell//2;y=l.yours+l.cell//2 end
 on_touch(x,y,'tap')
end
if M.cols==8 then local m=M.moves(app.state())[1];tapCell(m:byte(1));shot('selected');tapCell(m:byte(2))
else tapCell(M.cols==7 and 4 or 1)end
shot('firstmove')
local ticks=0
while app.state().status==0 and app.state().turn==2 do on_update();ticks=ticks+1;assert(ticks<100000)end
shot('reply')
local rounds=0
while app.state().status==0 do
 local g=app.state()
 if g.turn==2 then on_update()else
  if M.cols==8 then local m=M.choose(g);assert(m);tapCell(m:byte(1));tapCell(m:byte(2))
  else local c=M.cols==3 and M.choose(g)or 1
   if M.cols==7 then while g.b[35+c]~=0 do c=c%7+1 end end
   tapCell(c)
  end
 end
 rounds=rounds+1;assert(rounds<100000,'game failed to finish')
end
shot('result');local l=app.layout();assert(l.done and l.again);on_touch(l.done.x+10,l.done.y+10,'tap');on_draw();shot('record-menu')
on_exit()
