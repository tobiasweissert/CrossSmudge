assert(smudge.get_device()=='sticky','production Sticky identity required')
local before=smudge.get_orientation()
local app=dofile('main.lua');on_init();on_draw();capture('menu.pgm')
local id=APP;local M=dofile('core.lua');local V,g=app.view,app.state()
local expected=id=='hearts'and'landscape_ccw'or'portrait'
assert(smudge.get_orientation()==expected,'original forced orientation not preserved')
local function tap(op,value)
 for _,c in ipairs(V.controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing drawn control '..op)
end
if id=='jaipur'then local raw=M.encode(app.state());tap('nearby');assert(V.unavailable and not V.playing());capture('nearby-unavailable.pgm');assert(M.encode(app.state())==raw);tap('closeNearby')end
tap('help',id=='hearts'and 1 or nil)
for page=1,(id=='hearts'and 4 or 8)do
 on_draw();capture('help-'..page..'.pgm')
 if id=='hearts'then tap(page==4 and'menu'or'help',page==4 and nil or page+1)else tap('advance')end
end
if id=='hearts'then tap('resume')else tap('new')end
on_draw();capture('board.pgm');assert(smudge.write_file('state.txt',M.encode(app.state()),false))
on_exit();assert(smudge.get_orientation()==before,'orientation did not restore')
