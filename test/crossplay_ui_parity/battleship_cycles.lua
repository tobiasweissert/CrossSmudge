-- Complete two real touch games with the fixed clock/seed; record identity must
-- distinguish explicit New Game while not counting redraw/reload twice.
local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:match('%.dat$')then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local function win()
 tap(app.layout().view);local g=app.state();assert(g.phase==1)
 for cell=1,100 do if g.b:sub(cell,cell)~='0'then
  local l=app.layout();on_touch(l.x+(cell-1)%10*l.cell+l.cell//2,l.y+(cell-1)//10*l.cell+l.cell//2,'tap');on_draw()
  tap(app.layout().view);on_update();on_draw()
 end end
 assert(g.status==1,'actual aimed shots did not win');return g.a..g.b..g.sa..g.sb
end
local first=win();assert(app.records().played==1 and app.records().won==1)
local M=dofile('core.lua');local durable=dofile('storage.lua').load(function(raw)local data=raw:match('^1|(.*)$');return data and M.decode(data)end)
assert(durable and durable.status==1 and saved['native-stats-a.dat'],'terminal callback did not persist the completed game and record')
on_button('back',true);on_draw();on_button('confirm',true);on_draw()
assert(app.state().phase==0 and app.layout().placement)
assert(win()==first,'fixed-seed fixture unexpectedly changed; repeat identity case not covered')
assert(app.records().played==2 and app.records().won==2 and app.records().streak==2,'explicit same-seed new game counted as old completion')
on_draw();on_exit();app=dofile('main.lua');on_init();on_draw()
assert(app.records().played==2,'reload duplicated second completion')
on_exit();print('Two actual touch games/NewGame/same-seed identity/one completion each/reload passed')
