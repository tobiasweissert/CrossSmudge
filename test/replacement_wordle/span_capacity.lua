-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local small=smudge.get_memory().lua_max_kb<128
local app=dofile('main.lua');on_init();on_draw();on_update();on_exit()
if small then assert(app.state().day==-1 and app.state().notice:find('PSRAM',1,true),'wide result span needs an explicit preservation notice')
else assert(app.state().day==1931) end
assert(app.state().results:byte(1)==129 and app.resultOffset()==0,'old result changed')
