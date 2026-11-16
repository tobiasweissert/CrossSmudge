local read,write=smudge.read_file,smudge.write_file;local saved={};local fail=false
smudge.get_date=function()return {year=2026,month=10,day=2}end
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:sub(1,1)=='/'then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)
 if fail and path:find('crossplay-day-',1,true)then return false end
 saved[path]=value;return true
end
local app=dofile('main.lua');local day=app.core.dayIndex(2026,10,2)
saved['wordle.ans']=string.rep('.',(day-1)*5)..'APPLEAPPLE';saved['wordle.words']='ALLEYAPPLECRANESTONE'
on_init();on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
tap(app.layout().keys[1]);assert(app.state().game.typing=='Q')
local before=app.core.save(app.state().game);local results=app.state().results;fail=true
on_button('back',true);on_draw();tap(app.layout().menu[2])
for _,b in ipairs(app.layout().calendar.cells)do if b.index==day-1 then tap(b);break end end
assert(app.state().day==day and app.state().game.typing=='Q','failed journal write allowed archive switch')
assert(app.core.save(app.state().game)==before and app.state().results==results,'failed switch altered puzzle/statistics')
fail=false;on_button('back',true);on_draw();tap(app.layout().hero);on_update();on_exit()
app=nil;collectgarbage('collect');app=dofile('main.lua');on_init();on_draw()
assert(app.state().day==day and app.state().game.typing=='Q','write retry lost saved typing')
on_exit();print('Failed archive-switch write preserves game and successful retry reopens')
