local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:match('%.dat$')then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local w,h=smudge.get_bounds();local top=h-158;local row=false;local rect=smudge.rect
smudge.rect=function(x,y,width,height,fill,black,...)
 if x==16 and y==top and width==w-32 and height==62 and fill and black then row=true end
 return rect(x,y,width,height,fill,black,...)
end
local app=dofile('main.lua');on_init();on_draw()
assert(row,'Original Battleship front door has New Game and Play Nearby rows')
if capture then capture('battleship-menu.pgm')end
-- Nearby must report the real absent host service, never fake a match/search.
on_button('down',true);on_button('confirm',true);on_draw()
local g=app.state();assert(g.phase==0 and g.ship==1)
on_button('back',true);on_draw();on_button('confirm',true);on_draw()
assert(app.layout().placement,'New Game did not open native fleet editor')
on_exit()
