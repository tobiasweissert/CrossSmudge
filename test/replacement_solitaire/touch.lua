-- Direct taps use actual drawn card geometry, with no keyboard fallback.
smudge.get_device=function() return 'sticky' end
local app=dofile('main.lua');on_init();on_draw()
local C=dofile('cp_core.lua')
local function fixture(piles)
 local g=app.state();g.piles={};g.moves=0;g.history={};g.drawThree=false
 local used={}
 for p=0,12 do g.piles[p]=piles[p] or {};for _,c in ipairs(g.piles[p]) do assert(not used[c&127]);used[c&127]=true end end
 for s=0,3 do for r=0,12 do local c=(s<<4)|r;if not used[c] then g.piles[0][#g.piles[0]+1]=c end end end
 on_draw();return g
end
local function tap(p,i)
 local l=app.layout();local col=p>=6 and p-6 or p<=1 and p or p+1
 local y=p>=6 and l.tab+(i or math.max(1,#app.state().piles[p]))*l.pileSteps[p]-l.pileSteps[p]+1 or l.top+2
 on_touch(l.x+col*(l.cw+6)+l.cw//2,y,'tap');on_draw()
end
local function toolbar(i) local b=app.layout().buttons[i];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw() end
-- Same-card second tap must deselect, not silently reselect and move later.
local g=fixture({[6]={140}});tap(6);tap(6);tap(7)
assert(g.moves==0 and #g.piles[6]==1,'second tap must deselect')
tap(6);tap(7);assert(g.moves==1 and #g.piles[7]==1,'king to empty column')
toolbar(3);assert(g.moves==0 and #g.piles[6]==1,'touch Undo')
-- Exposed card in an overlapping run, followed by destination card.
g=fixture({[6]={2,155,138},[7]={140}});tap(6,2);tap(7)
assert(g.moves==1 and #g.piles[7]==3 and g.piles[6][1]==130,'run and reveal')
-- Invalid destination and face-down selection must not mutate the board.
g=fixture({[6]={155},[7]={140}});local before=C.save(g);tap(6);tap(8)
assert(C.save(g)==before,'invalid empty destination changed board')
g=fixture({[6]={27},[7]={140}});before=C.save(g);tap(6);tap(7)
assert(C.save(g)==before,'face-down card moved');tap(7) -- deselect destination
-- Waste -> empty foundation requires a source tap followed by destination tap.
g=fixture({[1]={128}});tap(1);assert(g.moves==0,'source tap must select, not auto-move');tap(2)
assert(g.moves==1 and #g.piles[2]==1 and #g.piles[1]==0)
g=fixture({[2]={144},[6]={129}});tap(2);tap(6)
assert(g.moves==1 and #g.piles[2]==0 and #g.piles[6]==2,'foundation to tableau')
-- Crowded 24-card column selects the visible run at its actual rendered step.
local column={};for s=0,3 do for r=0,12 do local c=(s<<4)|r;if c~=27 and c~=10 and c~=12 and #column<22 then column[#column+1]=c end end end
column[23]=155;column[24]=138
g=fixture({[6]=column,[7]={140}});tap(6,23);tap(7)
assert(g.moves==1 and #g.piles[6]==22 and #g.piles[7]==3,'crowded run hit test')
-- Modal cancel and deal are both operable by touch.
local saved=C.save(g);toolbar(2);local l=app.layout();on_touch(l.w//4,l.h//2,'tap');on_draw()
assert(C.save(g)==saved,'reset cancel mutated board')
toolbar(2);on_touch(l.w*3//4,l.h//2,'tap');on_draw();g=app.state()
assert(g.drawThree and g.moves==0 and #g.piles[0]==24,'touch reset confirmation')
for i=1,8 do tap(0) end;assert(#g.piles[0]==0 and #g.piles[1]==24)
tap(0);assert(#g.piles[0]==24 and #g.piles[1]==0,'touch recycle')
-- All cards face up endgame: tap Finish must complete the game.
g=fixture({});for p=0,12 do g.piles[p]={} end
for s=0,3 do for r=0,11 do g.piles[2+s][r+1]=(s<<4)|r|128 end;g.piles[6+s]={(s<<4)|12|128} end
on_draw();toolbar(4);assert(C.won(g),'touch auto-finish');on_exit()
print('direct card/run/deselect/empty/invalid/overlap/reset/recycle/finish taps passed')
