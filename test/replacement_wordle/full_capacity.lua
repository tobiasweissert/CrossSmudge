-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- Maximum accepted results payload under the PSRAM/Sticky budget. No giant
-- dictionary is retained in Lua: app dictionary probes are five bytes each.
smudge.get_date=function()return {year=2026,month=10,day=2} end
local J=dofile('cp_journal.lua')
local j=J.open('wordle',8200,function(s)return #s==8194 end,'results-')
assert(j:write('0\n'..string.rep(string.char(130),8192)));j=nil;J=nil;collectgarbage('collect')
local app=dofile('main.lua');on_init();on_draw()
local stats=app.core.statsFor(app.state().results,1931-app.resultOffset())
assert(stats.won==8192 and stats.wins[2]==8192)
on_exit();print('Wordle maximum8192-day results fixture passed at Sticky budget')
