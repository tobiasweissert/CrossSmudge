assert(smudge.get_device()=='sticky')
local read=smudge.read_file;local files={};smudge.millis=function()return 123456 end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local s=files[path];return s and s:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,s)files[path]=s;return true end
local app=dofile('main.lua');on_init();on_draw();local start
for _,hit in ipairs(app.view.hit)do if hit.action==4 then start=hit.box end end;assert(start)
local x,y=start.x+start.w//2,start.y+start.h//2
on_touch(x,y,'tap');assert(app.state().phase==2,'first real Start must enter the hidden ready card')
on_touch(x,y,'tap');assert(app.state().phase==2,'queued old menu Start bypassed unseen outward-ready card and exposed word/started clock')
on_draw();on_exit()
