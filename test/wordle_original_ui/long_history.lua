-- A real validated old history journal, not direct statistics setters.
local read=smudge.read_file;local saved={}
smudge.get_date=function()return {year=2026,month=10,day=2}end
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:sub(1,1)=='/'then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');local day=app.core.dayIndex(2026,10,2)
saved['wordle.ans']=string.rep('.',day*5)..'APPLE';saved['wordle.words']='ALLEYAPPLECRANESTONE'
local J=dofile('cp_journal.lua');local journal=J.open('wordle',8200,function()return true end,'results-')
assert(journal:write((day-999)..'\n'..string.rep(string.char(129),1000)))
on_init();on_draw();on_button('back',true)
local bitmap=smudge.bitmap;local width,height=smudge.get_bounds();local countX=width<height and 420 or 348
local top=width<height and 285 or 265;local observed=false
smudge.bitmap=function(x,y,w,h,...)
 if x>=countX and x<countX+44 and y>=top-10 and y<top+30 then
  observed=true;assert(x+w<=countX+44,'long native count overflows its actual44px glyph box')
 end
 return bitmap(x,y,w,h,...)
end
on_draw();assert(observed,'long counter glyph path not exercised');capture('menu-long.pgm')
local stats=app.core.statsFor(app.state().results,day-app.resultOffset())
assert(stats.played==1000 and stats.won==1000 and stats.streak==1000 and stats.wins[1]==1000,'real old journal was truncated or reset')
on_exit();app=nil;collectgarbage('collect');app=dofile('main.lua');on_init();on_draw()
assert(app.core.statsFor(app.state().results,day-app.resultOffset()).wins[1]==1000,'reopen lost long history')
on_exit();print('Actual1000dayjournal/nativecounttruncation/reopen passed')
