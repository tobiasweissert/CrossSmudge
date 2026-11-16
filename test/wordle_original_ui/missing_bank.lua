local read=smudge.read_file;local saved={}
smudge.get_date=function()return {year=2026,month=10,day=2}end
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:sub(1,1)=='/'then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();local prior=app.core.save(app.state().game)
on_button('back',true);on_draw();capture('menu-missing.pgm');local l=app.layout()
assert(not l.hero,'missing daily archive fabricated a today puzzle')
local b=l.menu[2];on_touch(b.x+4,b.y+4,'tap');on_draw();assert(app.layout().screen=='menu','unavailable archive changed screen')
b=app.layout().menu[3];on_touch(b.x+4,b.y+4,'tap');on_draw();capture('import-route.pgm')
assert(app.layout().screen=='import','missing bank has no actual import instructions')
assert(app.core.save(app.state().game)==prior,'import instructions replaced practice game')
on_button('back',true);on_draw();assert(app.layout().screen=='menu','import instructions cannot return')
on_exit();print('Missing bank preserves games and exposes real SD import route')
