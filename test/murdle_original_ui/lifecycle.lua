local read,write=smudge.read_file,smudge.write_file;local saved={};local reject=false;local now=10000
smudge.millis=function()return now end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local raw=saved[path];return raw and raw:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,data)if path:match('%.dat$')then if reject then return false end;saved[path]=data;return true end;return write(path,data)end
local app
local function boot()app=dofile('main.lua');on_init();on_draw()end
local function target(action,value)
 for _,v in ipairs(app.hits())do if v.action==action and(value==nil or v.value==value)then return v.box end end;error('missing action '..action)
end
local function tap(action,value)local b=target(action,value);on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
boot();local oldid=app.state().id;tap('play');tap('face',2);local b=target('cell');on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();local marks=table.concat(app.state().marks);on_exit()
boot();assert(table.concat(app.state().marks)==marks);local durable=app.core.encode(app.state());tap('new');tap('menu');assert(app.core.encode(app.state())==durable,'cancel replaced old case')
tap('new');reject=true;tap('fresh');on_exit();reject=false;boot();assert(app.state().id==oldid and table.concat(app.state().marks)==marks,'failed new-case save lost old durable board')
tap('new');local b=target('fresh');on_touch(b.x+b.w//2,b.y+b.h//2,'tap');local newid=app.state().id;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');assert(app.state().id==newid,'queued stale confirm replaced two cases before repaint');on_draw();on_exit()
boot();assert(app.state().id==newid);on_exit();print('Actual Murdle cancel/reopen/failed new case/queued-contact lifecycle passed')
