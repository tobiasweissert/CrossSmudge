local read,write=smudge.read_file,smudge.write_file;local saves={}
smudge.read_file=function(path,cap,offset)
 if saves[path]then return saves[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:match('%.dat$')then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saves[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();capture('menu.pgm')
on_button('confirm',true);on_draw();capture('board.pgm')
on_button('back',true);on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();capture('help1.pgm')
on_button('confirm',true);on_draw();capture('help2.pgm')
on_button('confirm',true);on_draw();capture('help3.pgm');on_exit()
