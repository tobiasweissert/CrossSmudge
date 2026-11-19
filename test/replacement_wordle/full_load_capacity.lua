-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- Journal prepared outside the VM: this measures real loading, not fixture allocation.
smudge.get_date=function()return {year=2026,month=10,day=2} end
local small=smudge.get_memory().lua_max_kb<128
local app=dofile('main.lua');on_init();on_draw();on_update();on_exit()
if small then
 assert(app.state().day==-1 and app.state().notice:find('PSRAM',1,true),'large history must safely explain refusal and retain practice')
 assert(#app.state().results==0)
else
 assert(app.core.statsFor(app.state().results,1931-app.resultOffset()).won==8192)
end
print('Maximum history '..(small and 'safely refused before load' or 'loaded'))
