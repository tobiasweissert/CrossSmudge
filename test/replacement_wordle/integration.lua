-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local base='/.smudge/saved_data/wordle/'
local app=dofile('main.lua');local C=app.core
local today=C.dayIndex(2026,10,2)
assert(smudge.write_file('wordle.ans',string.rep('.',(today-1)*5)..'CRANEAPPLE'))
assert(smudge.write_file('wordle.words','ALLEYAPPLECRANESTONE'))
assert(smudge.write_file(base..'wordle_state.dat','APPLE;;ZZZZZ;27'))
on_init();if app.layout().keys then smudge.header=function()error('legacy Wordle UI rendered')end;smudge.bitmap=function(x,y,w,h,data,white,gray)assert(not gray,'pale hint glyph')end end;on_draw()
assert(app.state().day==today and app.state().game.answer=='APPLE')
assert(smudge.read_file(base..'wordle_state.dat',100)=='APPLE;;ZZZZZ;27')
assert(smudge.read_file(base..'legacy-wordle_state.bak',100)=='APPLE;;ZZZZZ;27')
local labels={'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K','L','Del','Z','X','C','V','B','N','M','Enter','Clear','Menu'}
local function tap(n)
 local l=app.layout();local x=12+(n-1)%10*l.kw+(l.kw-3)//2;local y=l.ky+(n-1)//10*(l.kh+4)+l.kh//2
 if l.keys then
  local b=l.keys[n==29 and 20 or n]
  if n==30 then on_button('back',true);return end
  x,y=b.x+b.w//2,b.y+b.h//2
  if n==29 then on_long_press(x,y);return end
 end
 if on_tap then on_tap(x,y) end;if on_touch then on_touch(x,y,'tap') end
end
local function typeWord(w)
 for ch in w:gmatch('.') do for i,key in ipairs(labels)do if key==ch then tap(i);break end end end
end
local function menu(n)
 on_button('back',true);on_draw();local l=app.layout();local b=l.menu and l.menu[n];on_touch(b and b.x+2 or l.mx+10,b and b.y+2 or l.my+10+(n-1)*38,'tap');on_draw()
end
tap(1);assert(app.state().game.typing=='Q','physical dispatch typed twice');tap(20)
typeWord('ZZZZZ');tap(28);assert(#app.state().game.rows==0 and app.state().game.typing=='ZZZZZ');tap(29)
typeWord('ALLEY');tap(28);assert(#app.state().game.rows==1 and app.state().game.keys.L==3)
typeWord('APPLE');tap(28);assert(app.state().game.status=='Won');on_update()
local stats=C.statsFor(app.state().results,today-app.resultOffset());assert(stats.won==1 and stats.played==1 and stats.wins[2]==1)
menu(3);assert(app.state().day==today-1 and app.state().game.answer=='CRANE')
typeWord('CRANE');tap(28);on_update();assert(app.state().game.status=='Won')
stats=C.statsFor(app.state().results,today-app.resultOffset());assert(stats.won==1 and stats.played==1,'archive must not increment daily totals')
menu(2);assert(app.state().day==today and app.state().game.status=='Won' and #app.state().game.rows==2)
on_exit();tap(29);assert(app.state().game.status=='Won');app=nil;C=nil;collectgarbage('collect')
app=dofile('main.lua');on_init();on_draw();assert(app.state().day==today and app.state().game.status=='Won')
assert(app.core.statsFor(app.state().results,today-app.resultOffset()).won==1)
on_exit();app=nil;collectgarbage('collect')
local before=collectgarbage('count')
for i=1,8 do app=dofile('main.lua');on_init();on_draw();on_update();on_exit();app=nil;collectgarbage('collect') end
assert(collectgarbage('count')-before<8,'lifecycle retained memory')
assert(smudge.read_file(base..'wordle_state.dat',100)=='APPLE;;ZZZZZ;27')
print('Wordle daily/archive/touch/reject/stats/resume/legacy/lifecycle passed')
