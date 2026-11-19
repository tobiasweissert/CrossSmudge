local M=dofile('core.lua');local read=smudge.read_file;local saves={}
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)saves[path]=v;return true end
local app
local function shot(name,menu)
 on_draw();capture(name..'.pgm');local g=app.state();local b,w=M.score(g);local P=app.preferences
 assert(fixtureWrite(name..'.map',table.concat({g.n,g.turn,g.phase,g.moves,g.passes,g.last,g.captured[1],g.captured[2],g.komi,g.handicap,P.seat,g.solo,app.aim()or 0,b,w},' ')..'\n'..g.b..'\n'..g.dead..'\n'..(menu and('menu '..P.wins..' '..P.losses..' '..P.lastWon..' '..P.margin..' 1')or'')..'\n',false))
end
for _,n in ipairs({9,13,19})do for _,handicap in ipairs({0,5})do
 saves={};local g=M.new(123456,n,handicap);assert(dofile('storage.lua').save(M.encode(g)));app=dofile('main.lua');on_init();on_draw();shot('resume-'..n..'-'..handicap,true);on_button('confirm',true);on_draw();shot('board-'..n..'-'..handicap)
 local l=app.layout();local p=1;while not M.trial(app.state(),p)do p=p+1 end;local x=l.x0+(p-1)%n*l.pitch;local y=l.y0+(p-1)//n*l.pitch;on_touch(x,y,'tap');on_draw();assert(app.state().moves==0);shot('aim-'..n..'-'..handicap);on_touch(x,y,'tap');on_draw();assert(app.state().moves==1);shot('stone-'..n..'-'..handicap);on_exit()
end end
-- All five native settings remain reachable on a short landscape screen.
saves={};app=dofile('main.lua');on_init();on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='settings');on_button('confirm',true);on_draw();assert(app.preferences.nextMode==2)
for i=1,4 do on_button('down',true)end;on_button('confirm',true);on_draw();assert(app.preferences.nextBoard==13);on_button('confirm',true);on_draw();assert(app.preferences.nextBoard==19);on_button('back',true);on_button('confirm',true);on_draw();assert(app.state().n==19 and app.state().solo==0);on_exit()
