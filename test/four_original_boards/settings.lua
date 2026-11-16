local read=smudge.read_file;local saves={};smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)saves[path]=v;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='settings');on_button('confirm',true);on_draw();local P=app.preferences;assert(P.nextMode==2)
for row=1,(P.nextBoard and 5 or 3)do
 on_draw();capture('settings-'..row..'.pgm');assert(fixtureWrite('settings-'..row..'.map',table.concat({P.nextMode,P.nextSeat,P.nextHandicap or 0,P.nextBoard or 9,row-1},' ')..'\n',false));on_button('down',true)
end
on_button('back',true);on_draw();on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='nearby');capture('nearby-unavailable.pgm');on_button('back',true);on_draw();assert(app.screen()=='menu');on_exit()
