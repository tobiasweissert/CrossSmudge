assert(smudge.get_device()=='sticky','production identity required')
local read,write=smudge.read_file,smudge.write_file;local saved={};local now={year=2026,month=9,day=30}
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
on_init();on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local ids={A=11,P=10,L=19,E=3,Y=6,C=23,R=4,N=26,S=12,T=5,O=9}
local function submit(word)
 for c in word:gmatch('.')do tap(app.layout().keys[ids[c]])end
 tap(app.layout().keys[28])
end
for _,word in ipairs({'CRANE','STONE','ALLEY','APPLE'})do submit(word)end
assert(app.state().game.status=='Won');on_update()
now={year=2026,month=10,day=1};on_button('back',true);on_draw();tap(app.layout().hero)
for i=1,6 do submit('CRANE')end
assert(app.state().game.status=='Lost');on_update()
now={year=2026,month=10,day=2};on_button('back',true);on_draw();tap(app.layout().hero)
submit('ALLEY');submit('APPLE');assert(app.state().game.status=='Won');on_update()
on_button('back',true);on_draw();capture('menu-record.pgm')
local stats=app.core.statsFor(app.state().results,today-app.resultOffset())
assert(stats.played==3 and stats.won==2 and stats.streak==1 and stats.wins[2]==1 and stats.wins[4]==1,'native chart has invented stats')
tap(app.layout().menu[2]);capture('archive-record.pgm')
local before=app.state().results;local first
for _,b in ipairs(app.layout().calendar.cells)do if b.day==1 then first=b end end
assert(first and first.available);tap(first)
assert(app.state().game.status=='Lost'and #app.state().game.rows==6,'archive discarded saved losing game')
assert(app.state().results==before,'archive replay changed daily stats')
on_exit();app=nil;collectgarbage('collect');app=dofile('main.lua');on_init();on_draw()
assert(app.state().game.status=='Lost'and #app.state().game.rows==6,'reopen discarded archive game')
assert(app.core.statsFor(app.state().results,today-app.resultOffset()).played==3,'reopen counted completion twice')
on_exit();print('Real three-day Wordle win/loss chart/archive/reopen passed')
