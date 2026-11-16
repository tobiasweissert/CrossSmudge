local read=smudge.read_file;local saved={}
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then return saved[path]and saved[path]:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,data)saved[path]=data;return true end
local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw();if app.state().players==4 then for _,hit in ipairs(app.view.hit)do if hit.action=='players'and hit.value==1 then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();break end end end;capture('menu.pgm');on_exit();assert(smudge.get_orientation()==before,'app failed to restore orientation')
