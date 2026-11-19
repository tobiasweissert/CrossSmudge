local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end;if path:match('%.dat$')then return nil end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local function shot(name)
 on_draw();local g=app.state()
 assert(write('battle-'..name..'.state',table.concat({g.a,g.b,g.sa,g.sb,tostring(g.turn),tostring(g.last)},'\n'),false))
 capture('battle-'..name..'.pgm')
end
local function play(cell)
 local l=app.layout();on_touch(l.x+(cell-1)%10*l.cell+l.cell//2,l.y+(cell-1)//10*l.cell+l.cell//2,'tap');on_draw();tap(app.layout().view);on_update();on_draw()
end
tap(app.layout().view);shot('start');local g=app.state()
for cell=1,100 do if g.b:sub(cell,cell)=='0'then play(cell);break end end
shot('miss-reply')
for ship=1,5 do for cell=1,100 do if g.b:sub(cell,cell)==tostring(ship)then play(cell)end end;if ship==1 then shot('sunk-carrier')end end
assert(g.status==1);shot('win');assert(app.records().played==1)
tap(app.layout().view);assert(app.state().phase==0,'Play Again did not start native full fleet directly');tap(app.layout().view);shot('reset');g=app.state()
for pass=1,2 do for cell=1,100 do
 if g.status>0 then break end
 if (g.b:sub(cell,cell)=='0')==(pass==1)then play(cell)end
end end
assert(g.status==2,'actual public-observation AI did not win loss fixture');shot('loss')
assert(app.records().played==2 and app.records().won==1 and app.records().streak==0)
on_exit()
