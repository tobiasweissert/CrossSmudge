-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
local read,write=smudge.read_file,smudge.write_file
local base='/.smudge/saved_data/solitaire/'
assert(write(base..'game_state.dat','2;legacy state retained'))
assert(write(base..'games_won.dat','7'))
local app=dofile('main.lua');on_init();on_draw();if app.layout().menu then on_button('confirm',true);on_draw()end
assert(read(base..'game_state.dat',100)=='2;legacy state retained')
assert(read(base..'legacy-game_state.bak',100)=='2;legacy state retained')
assert(read(base..'legacy-games_won.bak',100)=='7')
local initial=app.state();assert(initial.moves==0 and #initial.piles[0]==24)
local l=app.layout();local x,y=l.x+l.cw//2,l.top+l.ch//2
-- Exactly the host's dispatch sequence, so accidentally providing two active
-- aliases would double-deal and fail this regression.
if on_tap then on_tap(x,y) end
if on_touch then on_touch(x,y,'tap') end
assert(initial.moves==1 and #initial.piles[0]==23,'one physical tap must draw exactly one')
on_update()
local C=dofile('cp_core.lua');local saved=C.save(initial);C=nil
on_exit();on_touch(x,y,'tap');on_button('confirm',true)
assert(initial.moves==1,'input after exit mutated state')
app=nil;initial=nil;collectgarbage('collect')
app=dofile('main.lua');on_init();on_draw();if app.layout().menu then on_button('confirm',true);on_draw()end
C=dofile('cp_core.lua');assert(C.save(app.state())==saved,'resume changed board');C=nil
-- Toolbar New 3 uses confirmation; Back cancels without touching the deal.
local function newThree()
 l=app.layout();if l.menuButton then local m=l.menuButton;on_touch(m.x+2,m.y+2,'tap');on_draw();l=app.layout() end
 if l.menu then
  if not l.drawThree then local mode=l.menu[#l.menu];on_touch(mode.x+2,mode.y+2,'tap');on_draw();l=app.layout()end
  local b=l.menu[2];on_touch(b.x+2,b.y+2,'tap')
 else local b=l.buttons[2];on_touch(b.x+2,b.y+2,'tap')end
end
newThree();on_button('back',true);on_draw()
assert(app.state().moves==1 and not app.state().drawThree)
newThree();on_button('confirm',true);on_draw()
assert(app.state().moves==0 and app.state().drawThree)
l=app.layout();on_touch(l.x+2,l.top+2,'tap');assert(#app.state().piles[0]==21)
on_update();on_exit();app=nil;collectgarbage('collect')
-- Repeated independent script loads in one VM catch retained app state/closures;
-- VM-per-launch coverage is also performed by the Python driver.
local start=collectgarbage('count')
for i=1,12 do local a=dofile('main.lua');on_init();on_draw();if a.layout().menu then on_button('confirm',true);on_draw()end;on_update();on_exit();a=nil;collectgarbage('collect') end
assert(collectgarbage('count')-start<8,'repeated load retained memory')
assert(read(base..'game_state.dat',100)=='2;legacy state retained')
print('Solitaire stock input/resume/new/cancel/legacy/repeated lifecycle passed')
