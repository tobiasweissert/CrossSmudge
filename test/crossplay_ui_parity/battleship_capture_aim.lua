local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end;if path:match('%.dat$')then return nil end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
tap(app.layout().view)
local function cell(at)local l=app.layout();on_touch(l.x+(at-1)%10*l.cell+l.cell//2,l.y+(at-1)//10*l.cell+l.cell//2,'tap');on_draw()end
local function shot(name,aim,repeated)
 local g=app.state();assert(write('battle-'..name..'.state',table.concat({g.a,g.b,g.sa,g.sb,tostring(g.turn),tostring(g.last),tostring(aim or 0),tostring(repeated or 0)},'\n'),false));capture('battle-'..name..'.pgm')
end
cell(1);shot('aim',1);cell(1);on_update();on_draw();cell(1);shot('already-fired',0,1);on_exit()
