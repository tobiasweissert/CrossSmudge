local M=dofile('core.lua');local read=smudge.read_file;local saves={}
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)saves[path]=v;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function shot(name,menu)
 on_draw();capture(name..'.pgm');local g=app.state();local b,w=M.score(g);assert(fixtureWrite(name..'.map',table.concat({g.n,g.turn,g.phase,g.moves,g.passes,g.last,g.captured[1],g.captured[2],g.komi,g.handicap,app.preferences.seat,g.solo,0,b,w},' ')..'\n'..g.b..'\n'..g.dead..'\n'..(menu and('menu '..app.preferences.wins..' '..app.preferences.losses..' '..app.preferences.lastWon..' '..app.preferences.margin..' 0')or'')..'\n',false))
end
local function tap(p)local g=app.state();local l=app.layout();local x=l.x0+(p-1)%g.n*l.pitch;local y=l.y0+(p-1)//g.n*l.pitch;on_touch(x,y,'tap');if g.phase==0 then on_touch(x,y,'tap')end;on_draw()end
tap(1);assert(app.state().moves==1);shot('played');on_update();on_draw();assert(app.state().moves==2);shot('ai-reply')
-- A controlled valid shared game reaches counting by actual PASS callbacks.
local g=M.new(123456);assert(M.play(g,1));assert(M.play(g,81));assert(M.decode(M.encode(g)));assert(dofile('storage.lua').save(M.encode(g)))
saves['native-view-a.dat']=nil;saves['native-view-b.dat']=nil;app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();local l=app.layout();on_touch(l.pass.x+10,l.pass.y+10,'tap');on_draw();on_touch(l.pass.x+10,l.pass.y+10,'tap');on_draw();assert(app.state().phase==1);shot('count')
tap(81);assert(M.at(app.state().dead,81)==1);shot('count-dead');l=app.layout();on_touch(l.other.x+10,l.other.y+10,'tap');on_draw();assert(app.state().phase==0 and app.state().dead==string.rep('0',81));l=app.layout();on_touch(l.pass.x+10,l.pass.y+10,'tap');on_touch(l.pass.x+10,l.pass.y+10,'tap');on_draw();assert(app.state().phase==1);l=app.layout();on_touch(l.pass.x+10,l.pass.y+10,'tap');on_draw();assert(app.state().phase==2);shot('result');assert(app.preferences.wins+app.preferences.losses==1);local l=app.layout();on_touch(l.other.x+10,l.other.y+10,'tap');on_draw();assert(app.screen()=='menu');shot('record',true);on_exit()
app=dofile('main.lua');on_init();on_draw();assert(app.preferences.wins+app.preferences.losses==1,'Go record duplicated reopen');on_exit()
