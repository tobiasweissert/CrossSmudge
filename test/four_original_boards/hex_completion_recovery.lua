local read=smudge.read_file;local saves={};local reject=false;smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)if reject and path:match('^state%-')then return false end;saves[path]=value;return true end
local function draw()on_draw();capture('progress.pgm')end
local app=dofile('main.lua');on_init();draw();on_button('down',true);on_button('down',true);on_button('confirm',true);draw();on_button('confirm',true);on_button('back',true);draw();on_button('confirm',true);draw();on_button('back',true);draw();on_exit()
local function finish()
 local l=app.layout();for row=0,10 do on_touch(l.x+2*l.a,l.y+l.half+2*l.half*row,'tap');if row<10 then on_touch(l.x+32*l.a,l.y+11*l.half+2*l.half*row,'tap')end end
 draw();assert(app.state().status==1)
end
on_button('confirm',true);draw();finish();assert(app.preferences.wins==1)
reject=true;on_exit();reject=false
app=dofile('main.lua');on_init();draw();assert(app.state().status==0 and app.preferences.wins==1);on_button('confirm',true);draw();finish();assert(app.preferences.wins==1,'recovery replay duplicated completed-game tally');on_exit()
