local read=smudge.read_file;local saved={};smudge.read_file=function(path,n,o)if saved[path]then return saved[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='help')
for page=1,5 do on_draw();capture('help'..page..'.pgm');on_button('confirm',true)end
assert(app.screen()=='menu');on_exit()
