local read,write=smudge.read_file,smudge.write_file;local saves={};local rejectCore=false;local rejectUI=false
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end
smudge.write_file=function(path,value)if rejectCore and path:match('^state%-')or rejectUI and path:match('^native%-view')then return false end;saves[path]=value;return true end
local M=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw();assert(app and app.preferences)
-- Settings retains original rows; White is a real existing-core turn routing choice.
on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='settings')
on_button('down',true);on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();assert(app.preferences.nextSeat==2)
on_button('back',true);on_draw();on_button('confirm',true);on_draw();assert(app.preferences.seat==2 and app.state().turn==1)
local ticks=0;while app.state().turn==1 do on_update();ticks=ticks+1;assert(ticks<10)end
on_draw();capture('go-white-ai-first.pgm');assert(app.state().moves==1)
local function moveFirstEmpty(start)
 local l=app.layout();local g=app.state();local at=start or 1;while not M.trial(g,at)do at=at+1 end
 local x=l.x0+(at-1)%g.n*l.pitch;local y=l.y0+(at-1)//g.n*l.pitch;on_touch(x,y,'tap');on_touch(x,y,'tap');return at
end
local at=moveFirstEmpty();assert(M.at(app.state().b,at)==2,'White tap did not play White')
on_button('back',true);on_draw();local payload=M.encode(app.state());on_exit()
app=dofile('main.lua');on_init();on_draw();assert(app.preferences.seat==2 and M.encode(app.state())==payload,'White seat/game did not reopen');on_button('confirm',true);on_draw()
while app.state().turn~=2 do on_update()end;on_draw();moveFirstEmpty();local inMemory=M.encode(app.state());assert(inMemory~=payload)
-- UI metadata succeeds, core write fails: previous matching White hint remains.
rejectCore=true;on_exit()
-- A second, different pending board must not overwrite the sole committed seat hint.
while app.state().turn~=2 do on_update()end;on_draw();moveFirstEmpty(60);on_exit();rejectCore=false
app=dofile('main.lua');on_init();on_draw();assert(M.encode(app.state())==payload and app.preferences.seat==2,'failed core save lost old game/seat')
on_button('confirm',true);on_draw();while app.state().turn~=2 do on_update()end;on_draw();moveFirstEmpty(70)
local previous={};for path,value in pairs(saves)do if path:match('^state%-')then previous[path]=value end end
rejectUI=true;on_exit();rejectUI=false
for path,value in pairs(saves)do if path:match('^state%-')then assert(previous[path]==value,'failed UI hint write advanced core journal')end end
app=dofile('main.lua');on_init();on_draw();assert(M.encode(app.state())==payload and app.preferences.seat==2)
on_exit()
