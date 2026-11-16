assert(smudge.get_device()=='sticky')
local read,write=smudge.read_file,smudge.write_file;local files={}
smudge.read_file=function(path,n,offset)if path:match('^art/')then return read(path,n,offset)end;return files[path]and files[path]:sub((offset or 0)+1,(offset or 0)+n)end
smudge.write_file=function(path,s)files[path]=s;return true end
smudge.millis=function()return 123456 end
local app=dofile('main.lua');on_init();on_draw();capture('entry.pgm')
assert(app.layout().start and #app.layout().start==3,'original supported Chess front door missing')
assert(app.state().home and not app.state().job,'opening the front door started an AI search')
local first=app.layout().start[1];on_touch(first.x+first.w//2,first.y+first.h//2,'tap');on_draw();capture('board.pgm')
assert(not app.state().home,'New Game did not open the board')
on_exit()
