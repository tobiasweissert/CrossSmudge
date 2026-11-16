local read,write=smudge.read_file,smudge.write_file;local saves={}
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)saves[path]=value;return true end
local M=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(at,hold)local l=app.layout();local x=l.x+(at-1)%8*l.cell+l.cell//2;local y=l.y+(at-1)//8*l.cell+l.cell//2;if hold then on_long_press(x,y)else on_touch(x,y,'tap')end end
local function shot(name,view)
 on_draw();capture(name..'.pgm');local g=app.state();local b={};for _,v in ipairs(g.b)do b[#b+1]=v end
 assert(fixtureWrite(name..'.map',table.concat(b)..'\n'..g.status..' '..(view or'board')..' '..(app.records and app.records().played or 0)..' '..(app.records and app.records().won or 0)..' 0\n',false))
end
tap(1,true);assert(app.state().status==0 and app.state().b[1]&4~=0);shot('flag');tap(1,true);assert(app.state().b[1]==0)
tap(1);assert(app.state().status==1 and M.adjacent(app.state(),1,1)==0,'first dig lost safe opening');shot('opened')
local mine;for at,v in ipairs(app.state().b)do if v&1~=0 then mine=at;break end end;assert(mine);tap(mine);assert(app.state().status==3);shot('boom-board')
local w,h=smudge.get_bounds();on_touch(30,h-40,'tap');on_draw();assert(app.screen()=='result','verdict did not open original result/statistics');shot('boom-result','result')
on_touch(30,h-40,'tap');on_draw();assert(app.screen()=='menu');assert(app.records().played==1 and app.records().won==0);shot('record-menu','menu')
on_button('confirm',true);on_draw();assert(app.state().status==0);tap(1)
for at=1,80 do local v=app.state().b[at];if v&1==0 and v&2==0 then tap(at)end end
assert(app.state().status==2);shot('clear-board');on_touch(30,h-40,'tap');on_draw();assert(app.screen()=='result');shot('clear-result','result')
assert(app.records().played==2 and app.records().won==1);on_exit();app=dofile('main.lua');on_init();on_draw();assert(app.records().played==2,'reopen duplicated/dropped actual completions')
