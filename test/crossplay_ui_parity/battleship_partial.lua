local read=smudge.read_file;local saved={};local reject=false
smudge.read_file=function(path,cap,offset)
 if path:match('^state%-')then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)if reject then return false end;saved[path]=value;return true end
local M=dofile('core.lua');local old=M.new(123456)
assert(M.action(old,1,1));assert(M.action(old,1,2))
local payload='1|'..M.encode(old);assert(dofile('storage.lua').save(payload))
local bytes={};for path,value in pairs(saved)do bytes[path]=value end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local g=app.state();assert('1|'..M.encode(g)==payload,'opening changed imported partial journal')
local l=app.layout();assert(l.placement.board:sub(1,5)=='11111','original placed carrier lost')
on_touch(l.x+l.cell//2,l.y+l.cell//2,'tap');on_draw()
on_touch(l.x+l.cell//2,l.y+l.cell//2,'tap');on_draw()
assert('1|'..M.encode(g)==payload,'staged editing changed old partial save')
on_exit();for path,value in pairs(bytes)do assert(saved[path]==value,'exit overwrote old partial journal')end
app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();l=app.layout()
on_touch(l.view.x+l.view.w//2,l.view.y+l.view.h//2,'tap');on_draw()
assert(app.state().phase==1 and M.valid(app.state()))
reject=true;on_exit();reject=false
app=dofile('main.lua');on_init();on_draw()
assert(app.state().phase==0 and app.state().ship==3 and '1|'..M.encode(app.state())==payload,'failed Ready save lost original partial journal')
print('Old partial save/import/staged edits/exit/failed writes preserve original bytes')
