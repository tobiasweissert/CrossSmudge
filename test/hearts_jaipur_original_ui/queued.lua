assert(smudge.get_device()=='sticky')
local app=dofile('main.lua');on_init();on_draw();local V,M=app.view,dofile('core.lua')
local function find(op)for i,c in ipairs(V.controls)do if c.op==op then return c,i end end;error('missing '..op)end
local function contact(c)on_touch(c.box.x+c.box.w//2,c.box.y+c.box.h//2,'tap')end
local function tap(op)local c=find(op);contact(c);on_draw()end
if APP=='hearts'then tap('resume');local c=find('act');contact(c);on_draw()else tap('new')end
tap('menu');local raw=M.encode(app.state());on_exit()
app=dofile('main.lua');on_init();on_draw();V=app.view;assert(M.encode(app.state())==raw,'saved game must reopen')
local clock=smudge.millis;local tick=clock()+23456;smudge.millis=function()return tick end
for _,method in ipairs({'touch','button'})do
 local c,i=find('new');V.focus=i;local a,b=smudge.read_file('state-a.dat',4097),smudge.read_file('state-b.dat',4097)
 if method=='touch'then contact(c);contact(c)else on_button('confirm',true);on_button('confirm',true)end
 assert(M.encode(app.state())==raw,'B12-01: unpainted confirmation discarded game')
 assert(smudge.read_file('state-a.dat',4097)==a and smudge.read_file('state-b.dat',4097)==b,'unpainted input changed journal bytes')
 on_draw();capture('discard-'..method..'.pgm');assert(V.confirm)
 -- Decline by resuming, return, ask again, paint, then allow exactly one reset.
 tap('resume');tap('menu');c,i=find('new');contact(c);on_draw();assert(V.confirm)
 -- A menu/back path cancels confirmation; no stale confirm should survive it.
 tap('resume');tap('menu');assert(not V.confirm)
end
-- Back exits rather than accepting the visible question; reopen retains the live game.
local c=find('new');contact(c);on_draw();on_button('back',true);app=dofile('main.lua');on_init();on_draw();V=app.view;assert(M.encode(app.state())==raw and not V.confirm)
local c,i=find('new');contact(c);on_draw();assert(V.confirm);V.focus=i
local write=smudge.write_file;smudge.write_file=function(path,value,...)if path:match('^state%-')then return false end;return write(path,value,...)end
tick=tick+12345
on_button('confirm',true);local fresh=M.encode(app.state());assert(fresh~=raw,'painted explicit confirm must reset');on_button('confirm',true);assert(M.encode(app.state())==fresh,'reset once only');on_draw();capture('save-failed.pgm');on_exit();smudge.write_file=write
app=dofile('main.lua');on_init();on_draw();V=app.view;assert(M.encode(app.state())==raw,'failed replacement save must preserve prior journal');assert(not V.confirm)
on_exit()
