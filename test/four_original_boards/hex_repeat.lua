local read=smudge.read_file;local saves={};smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)saves[path]=value;return true end
local function draw()on_draw();capture('progress.pgm')end
local app=dofile('main.lua');on_init();draw();on_button('down',true);on_button('down',true);on_button('confirm',true);draw();on_button('confirm',true);on_button('back',true);draw();on_button('confirm',true);draw();assert(app.preferences.nextMode==2)
local function tap(col,row)local l=app.layout();on_touch(l.x+2*l.a+3*l.a*col,l.y+l.half+2*l.half*row+l.half*col,'tap')end
for game=1,2 do
 draw();capture('start-'..game..'.pgm')
 for row=0,10 do tap(0,row);if row<10 then tap(10,row)end end
 assert(app.state().status==1);draw();capture('end-'..game..'.pgm');assert(app.preferences.wins+app.preferences.losses==game,'identical new game was not counted')
 if game==1 then local l=app.layout();on_touch(l.again.x+l.again.w//2,l.again.y+l.again.h//2,'tap');draw();assert(app.state().status==0)end
end
on_exit()
