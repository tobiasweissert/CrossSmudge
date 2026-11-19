assert(smudge.get_device()=='sticky','production identity required')
local read,write=smudge.read_file,smudge.write_file;local saved={};local now={year=2026,month=10,day=2}
smudge.get_date=function()return now end
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:sub(1,1)=='/'then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');local today=app.core.dayIndex(2026,10,2)
saved['wordle.ans']=string.rep('.',(today-2)*5)..'APPLEAPPLEAPPLE'
saved['wordle.words']='ALLEYAPPLECRANESTONE'
on_init();on_draw();local initial=app.core.save(app.state().game)
on_button('back',true);on_draw();capture('menu.pgm')
local l=app.layout()
assert(l.menu and #l.menu==3,'original Wordle menu has exactly HOW TO PLAY, ARCHIVE, and puzzle-import rows')
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
tap(l.menu[1]);capture('help.pgm');assert(app.layout().screen=='help','native help row did not open')
assert(app.core.save(app.state().game)==initial,'help changed puzzle')
on_button('back',true);on_draw();tap(app.layout().menu[2]);capture('archive.pgm')
l=app.layout();assert(l.calendar and #l.calendar.cells==42,'native archive needs six weeks of real dates')
local chosen
for _,b in ipairs(l.calendar.cells)do if b.day==1 then chosen=b end end
assert(chosen and chosen.available,'imported previous day missing')
tap(chosen);assert(app.state().day==today-1,'archive selected wrong day')
assert(app.core.save(app.state().game):find('APPLE',1,true),'archive changed answer source')
on_button('back',true);on_draw();local b=app.layout().hero;assert(b,'native today hero missing');tap(b)
assert(app.state().day==today,'native today hero selected wrong day')
on_button('back',true);on_draw();tap(app.layout().menu[2]);local old=app.layout().calendar.month
local nextBox=app.layout().calendar.nextMonth
if nextBox then tap(nextBox);assert(app.layout().calendar.month==old,'unavailable future month moved')end
on_exit();print('Original Wordle menu/help/calendar actual callbacks passed')
