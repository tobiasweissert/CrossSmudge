local read=smudge.read_file;local saved={};local reject=false
smudge.read_file=function(path,cap,offset)if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end;return read(path,cap,offset)end
smudge.write_file=function(path,value)if reject and path:find('ui%-records')then return false end;saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local l=app.layout();on_touch(l.x+3,l.top+3,'tap');on_draw();on_update()
on_button('back',true);on_draw();l=app.layout();local b=l.menu[2];on_touch(b.x+3,b.y+3,'tap');on_draw()
on_button('confirm',true);on_draw()
assert(app.records and app.records().played==1 and app.records().wins==0,'observed abandoned deal not recorded')
on_update();on_exit();app=dofile('main.lua');on_init();on_draw();assert(app.records().played==1,'record duplicated/lost on reopen')
on_button('confirm',true);on_draw();l=app.layout();on_touch(l.x+3,l.top+3,'tap');on_update();on_draw();reject=true
on_button('back',true);on_draw();b=app.layout().menu[2];on_touch(b.x+3,b.y+3,'tap');on_button('confirm',true);on_draw();assert(app.records().played==2);on_exit()
reject=false;app=dofile('main.lua');on_init();on_draw();assert(app.records().played==1,'failed UI write damaged last durable record')
on_exit();print('Observed-only original records, abandoned-deal confirmation, reopen and failed UI record write passed')
