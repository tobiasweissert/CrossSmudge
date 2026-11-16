local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end;if path:match('%.dat$')then return nil end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();capture('menu.pgm');on_button('confirm',true);on_draw();capture('board.pgm');on_exit()
