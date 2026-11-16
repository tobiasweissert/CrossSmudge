local read=smudge.read_file;local saves={}
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)saves[path]=value;return true end
local Y=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local function shot(name)
 on_draw();capture(name..'.pgm');local g=app.state();local values={g.turn,g.rolls,g.held}
 for i=1,5 do values[#values+1]=g.d[i]end;for player=1,2 do for c=1,14 do values[#values+1]=g.cards[player][c]end end
 assert(fixtureWrite(name..'.map',table.concat(values,' ')..'\n',false))
end
tap(app.layout().roll);assert(app.state().rolls==1);shot('rolled');tap(app.layout().dice[1]);assert(app.state().held&1~=0);shot('held')
tap(app.layout().roll);tap(app.layout().roll);assert(app.state().rolls==3);shot('spent')
local category=Y.choose_box(app.state().cards[1],app.state().d);local score=Y.score_for(app.state().cards[1],app.state().d,category);tap(app.layout().rows[category]);assert(app.state().cards[1][category]==score and app.state().turn==2,'original one-tap category take was not committed');shot('taken')
local ticks=0;while app.state().turn==2 and not Y.over(app.state())do on_update();ticks=ticks+1;assert(ticks<100000)end;shot('ai-reply')
on_exit();local encoded=Y.encode(app.state());app=dofile('main.lua');on_init();on_draw();assert(Y.encode(app.state())==encoded,'save/reopen changed scorecard')
