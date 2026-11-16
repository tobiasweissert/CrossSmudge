local read=smudge.read_file;local saves={};local reject=false;smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)if reject and path:match('^state%-')then return false end;saves[path]=value;return true end
local function draw()on_draw();capture('progress.pgm')end
local app=dofile('main.lua');on_init();draw();on_button('down',true);on_button('down',true);on_button('confirm',true);draw();on_button('confirm',true);on_button('back',true);draw();on_button('confirm',true);draw()
local function finish(col,other)
 local l=app.layout();for row=0,10 do on_touch(l.x+2*l.a+3*l.a*col,l.y+l.half+2*l.half*row+l.half*col,'tap');if row<10 then on_touch(l.x+2*l.a+3*l.a*other,l.y+l.half+2*l.half*row+l.half*other,'tap')end end;draw();assert(app.state().status==1)
end
finish(0,10);on_exit();local l=app.layout();on_touch(l.again.x+10,l.again.y+10,'tap');draw();finish(1,9);assert(app.preferences.wins==2)
reject=true;on_exit();reject=false
for iteration=1,2 do app=dofile('main.lua');on_init();draw();assert(app.preferences.wins==2,'restored earlier completed core save duplicated lifetime record');on_exit()end
