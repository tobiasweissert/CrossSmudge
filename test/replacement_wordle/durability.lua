-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local app=dofile('main.lua');local day=app.core.dayIndex(2026,10,2)
assert(smudge.write_file('wordle.ans',string.rep('.',day*5)..'APPLE'))
assert(smudge.write_file('wordle.words','APPLECRANE'))
on_init();on_draw();app.state().game.typing='APPLE'
local write=smudge.write_file;local fail=true
smudge.write_file=function(path,body,append)if fail and path:find('crossplay-results-',1,true) then return false end;return write(path,body,append) end
local l=app.layout();on_touch(14+7*l.kw,l.ky+2*(l.kh+4)+2,'tap');assert(app.state().game.status=='Won')
fail=false;on_update();on_exit();app=nil;collectgarbage('collect')
app=dofile('main.lua');on_init()
assert(app.core.statsFor(app.state().results,day-app.resultOffset()).won==1,'transient result write failure permanently lost a saved daily win')
on_exit();print('Wordle failed-result write retry passed')
