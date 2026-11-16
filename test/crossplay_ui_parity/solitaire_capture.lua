local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();if app.layout().menu then on_button('confirm',true);on_draw()end;capture('solitaire-board.pgm')
local g=app.state();local parts={'seed'..g.seed..' moves'..g.moves}
for p=0,12 do local a={};for _,card in ipairs(g.piles[p])do a[#a+1]=card..','end;parts[#parts+1]=table.concat(a)end
assert(write('solitaire-native-state.txt',table.concat(parts,'|')..'\n',false))
on_exit()
