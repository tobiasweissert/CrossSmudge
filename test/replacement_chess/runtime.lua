-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
local files={}
local base='/.smudge/saved_data/chess/'
local old=string.rep('A',76);files[base..'game.dat']=old
local assetRead=smudge.read_file
smudge.read_file=function(p,n,offset) if p:match('^art/')then return assetRead(p,n,offset)end;assert(n and n<=11065 and offset==0,'unbounded read');local s=files[p];return s and s:sub(1,n) end
smudge.write_file=function(p,s,append) assert(p~=base..'game.dat','old save overwritten');assert(append==false and #s<=11064);files[p]=s;return true end
local app=dofile('main.lua');on_init();on_draw();if app.layout().start then local b=app.layout().start[1];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local g=app.state();local C=app.core
assert(C and g.notice:find('Old save'),'legacy notice')
assert(files[base..'game.dat']==old and files[base..'legacy-game.bak']==old)
local function tap(s,event)
 local l=app.layout();local r,c
 if g.mode==2 or g.mode==3 and not g.position.whiteToMove then r=s//8;c=7-(s%8) else r=7-s//8;c=s%8 end
 on_touch(l.x+c*l.size+l.size//2,l.y+r*l.size+l.size//2,event or 'tap')
end
local function menu(i)
 if g.home then on_button('confirm',true);on_draw()end
 if not g.menu then local b=app.layout().gear;on_touch(b.x+b.w//2,b.y+b.h//2,'tap')end
 on_draw();for _=1,12 do if app.layout().buttons[i]then break end;on_button('down',true);on_draw()end;local b=app.layout().buttons[i];assert(b);on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()
end
local original=C.fen(g.position)
tap(12,'move');assert(not g.selected and C.fen(g.position)==original,'non-tap event changed game')
on_button('confirm',false);assert(not g.selected,'release activated button');on_button('confirm',true);assert(g.selected==12);on_button('left',true);on_button('left',true);on_button('confirm',true);assert(g.position.squares[28]==1 and g.job,'legal button move did not queue AI')
local moved=C.fen(g.position);on_update();assert(C.fen(g.position)==moved,'AI search changed visible board mid-step')
local gear=app.layout().gear;on_touch(gear.x+gear.w//2,gear.y+gear.h//2,'tap');assert(g.menu and not g.job);for i=1,5 do on_update() end;assert(C.fen(g.position)==moved)
menu(1);assert(g.job);local steps=0
while g.job do on_update();steps=steps+1;assert(steps<20000,'AI stalled') end
assert(g.position.whiteToMove and #g.history==2,'AI failed to answer');on_draw()
menu(5);assert(C.fen(g.position)==original,'undo did not restore human turn')
menu(3);menu(1);assert(g.mode==3);menu(2);assert(not g.job and #g.history==0)
-- Entire game via rendered touch coordinates.
for _,m in ipairs({{13,21},{52,36},{14,30},{59,31}}) do tap(m[1]);tap(m[2]);on_draw() end
assert(g.over=='Checkmate','Fools Mate UI result')
-- Full promotion selection through rendered overlay.
g.position=assert(C.parseFen('7k/P7/8/8/8/8/7p/K7 w - - 0 1'));g.history={};g.moves=C.legal(g.position);g.over=nil;g.selected=nil;g.last=nil
on_draw();tap(48);tap(56);assert(g.promotion and #g.promotion==4);on_draw()
local b=app.layout().buttons[4];on_touch(b.x+10,b.y+10,'tap');assert(g.position.squares[56]==2,'knight underpromotion')
on_update();on_exit();local saved=C.fen(g.position)
app=nil;g=nil;C=nil;collectgarbage('collect')
app=dofile('main.lua');on_init();g=app.state();C=app.core;assert(C.fen(g.position)==saved,'resume differs');on_draw()
-- Repeated load/exit with journal bytes carried between VM-level app instances.
for i=1,12 do on_exit();app=nil;g=nil;C=nil;collectgarbage('collect');app=dofile('main.lua');on_init();g=app.state();C=app.core;on_draw();assert(C.fen(g.position)==saved) end
on_draw();menu(6);assert(g.job and g.hint);local before=C.fen(g.position);on_update();on_exit();assert(not g.job);for i=1,8 do on_update() end;assert(C.fen(g.position)==before and g.closed,'exit left stale search')
assert(files[base..'game.dat']==old and files[base..'legacy-game.bak']==old,'legacy data altered')
print('touch/buttons, AI/cancel/undo, complete game, underpromotion, save/resume,12 reloads and exit passed')
