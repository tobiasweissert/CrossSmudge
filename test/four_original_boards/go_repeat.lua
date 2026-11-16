local M=dofile('core.lua');local read=smudge.read_file;local saves={};local fail=false
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)if fail and path:match('^state%-')then return false end;saves[path]=v;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();on_button('confirm',true);on_button('back',true);on_draw();on_button('confirm',true);on_draw();on_exit();local empty=M.encode(app.state())
local function finish()
 on_button('confirm',true);on_button('confirm',true);on_draw();assert(app.state().phase==1);on_button('confirm',true);on_draw();assert(app.state().phase==2)
end
finish();assert(app.preferences.wins+app.preferences.losses==1);fail=true;on_exit();fail=false
app=dofile('main.lua');on_init();on_draw();assert(M.encode(app.state())==empty);on_button('confirm',true);on_draw();finish();assert(app.preferences.wins+app.preferences.losses==1,'replayed failed completion counted twice');on_exit()
on_button('confirm',true);on_draw();assert(app.state().phase==0);finish();assert(app.preferences.wins+app.preferences.losses==2,'identical new game was not counted');on_exit();app=dofile('main.lua');on_init();on_draw();assert(app.preferences.wins+app.preferences.losses==2);on_exit()
