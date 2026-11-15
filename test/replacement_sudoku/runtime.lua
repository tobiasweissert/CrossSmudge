-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- Exercise production callbacks and state using verified stock API signatures.
local files, writeCount, now = {}, 0, 1000
local base='/.smudge/saved_data/sudoku/'
local legacy='1,1,1,'..string.rep('0,1,0,0,',80)..'0,1,0,0'
files[base..'state.dat']=legacy
local assetRead=smudge.read_file
smudge.read_file=function(path,cap,offset)
  if path:match('^art/') then return assetRead(path,cap,offset) end
  assert(path:sub(1,#base)==base and cap<=4161, 'unbounded or nonlocal state read')
  local s=files[path];if not s then return nil end
  return s:sub((offset or 0)+1,(offset or 0)+cap)
end
smudge.write_file=function(path,s,append)
  assert(path:sub(1,#base)==base and path~=base..'state.dat' and not append)
  files[path]=s;writeCount=writeCount+1;return true
end
smudge.millis=function() return now end
local app=dofile('main.lua')
on_init();on_draw()
local function state() return app.state() end
local function taprect(b) on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw() end
local function button(n) taprect(app.layout().buttons[n]) end
local function tapcell(i,event)
  local l=app.layout()
  on_touch(l.x+((i-1)%9)*l.size+l.size//2,l.y+((i-1)//9)*l.size+l.size//2,event or 'tap')
  on_draw()
end
local function digit(d)
  local l=app.layout();taprect(l.keys[d])
end
local l=app.layout();taprect(l.difficulty);assert(#app.layout().menu==4);taprect(app.layout().menu[1])
taprect(app.layout().howto);on_button('back',true);on_draw();assert(not state().closed,'Back in help exited the app')
assert(files[base..'legacy-state.bak']==legacy and files[base..'state.dat']==legacy)
assert(state().notice:find('Old save'), 'missing migration notice')
taprect(app.layout().play);assert(state().job)
for i=1,3 do on_update() end
on_button('back',true);assert(not state().job and not state().given)
for i=1,4 do on_update() end
assert(not state().given, 'canceled task installed stale result')
on_draw();taprect(app.layout().play)
local frames=0
while state().job do on_update();frames=frames+1;assert(frames<10000,'generation never finishes') end
on_draw();assert(state().given and not state().menu)
local first
for i=1,81 do if state().given[i]==0 then first=i;break end end
assert(first)
local answer=state().solution[first]
digit(answer);on_long_press(app.layout().x+((first-1)%9)*app.layout().size+2,app.layout().y+((first-1)//9)*app.layout().size+2);on_draw()
assert(state().value[first]==0 and state().notes[first]==1<<(answer-1),'note mode failed')
tapcell(first,'hold');assert(state().notes[first]==0,'note toggle failed')
tapcell(first);assert(state().value[first]==answer)
tapcell(first);assert(state().value[first]==0,'matching digit did not clear')
tapcell(first);button(3);assert(state().value[first]==0,'undo failed')
on_button('confirm',false);assert(state().value[first]==0,'release changed cell')
on_button('confirm',true);assert(state().value[first]==answer,'button/touch disagree')
digit(answer%9+1);tapcell(first);button(2)
assert(state().notice=='Check this entry first' and state().value[first]~=answer)
button(3);assert(state().value[first]==answer)
tapcell(first,'hold');assert(state().value[first]==0,'optional hold adapter failed')
-- The opt-in long-press callback is the production pencil path.
button(2);assert(state().hinted and state().notice~='')
on_update()
local baseline=state().elapsed
now=now+5000;on_update();assert(state().elapsed==baseline+5,'running clock failed')
on_button('back',true);now=now+10000;on_update()
assert(state().elapsed==baseline+5,'paused menu counted elapsed time')
on_draw();taprect(app.layout().play)
now=now+1000;on_update();assert(state().elapsed==baseline+6,'resume counted paused time')
local clockWrites=writeCount
for i=1,10 do now=now+1000;on_update() end
assert(writeCount-clockWrites<=1,'clock writes save every second instead of debouncing')
on_exit();app=dofile('main.lua');on_init();on_draw()
assert(state().elapsed==baseline+16,'quiet elapsed time lost on exit')
-- Solve through the rendered keypad and board; givens must stay immutable.
for i=1,81 do
  if state().given[i]==0 and state().value[i]~=state().solution[i] then
    digit(state().solution[i]);tapcell(i)
  end
end
assert(state().solved,'full played game not recognized')
assert(state().recorded and state().counts[state().level+1]==1 and state().hintsUsed==1,'hinted solve was not recorded exactly once')
assert(state().best[state().level+1]==-1,'hinted solve set a best time')
on_update();local elapsed=state().elapsed
on_exit();local writesAtExit=writeCount
on_update();assert(writeCount==writesAtExit,'closed app ran stale save work')
app=dofile('main.lua');on_init();on_draw()
assert(state().solved and state().elapsed==elapsed,'save/resume failed')
assert(state().recorded and state().counts[state().level+1]==1,'resume counted the solved game again')
taprect(app.layout().status);on_draw();taprect(app.layout().grid);on_draw()
assert(files[base..'state.dat']==legacy and files[base..'legacy-state.bak']==legacy)
-- A checksum-valid but semantically invalid newest save must not displace the
-- prior valid game. These writes deliberately bypass the app's own validator.
local saved1,saved2=files[base..'crossplay-ui-1.dat'],files[base..'crossplay-ui-2.dat']
for _,bad in ipairs({'return os.execute("bad")', '0 999999999 0\n0\n0\n0\n0',
                     '0 0 0\n'..string.rep('0',81)..'\n'..string.rep('1',81)..'\n'..string.rep('0',81)..'\n'..string.rep('0,',80)..'0'}) do
  files[base..'crossplay-ui-1.dat'],files[base..'crossplay-ui-2.dat']=saved1,saved2
  local permissive=dofile('cp_journal.lua').open('sudoku',1024,function() return true end,'ui-')
  assert(permissive:write(bad))
  app=dofile('main.lua');on_init();on_draw()
  assert(state().solved and state().elapsed==elapsed,'invalid new save displaced valid fallback')
end
files[base..'crossplay-ui-1.dat'],files[base..'crossplay-ui-2.dat']=saved1,saved2
on_exit()
-- Import the installed first-candidate format without changing either old slot.
local g=state();local oldPayload=g.level..' '..g.elapsed..' '..(g.hinted and '1' or '0')..'\n'..table.concat(g.given)..'\n'..table.concat(g.solution)..'\n'..table.concat(g.value)..'\n'..table.concat(g.notes,',')
local old=dofile('cp_journal.lua').open('sudoku',1024,function()return true end)
assert(old:write(oldPayload));local old1,old2=files[base..'crossplay-1.dat'],files[base..'crossplay-2.dat']
files[base..'crossplay-ui-1.dat'],files[base..'crossplay-ui-2.dat']=nil,nil
app=dofile('main.lua');on_init();on_draw();on_update()
assert(state().solved and state().hintsUsed==-1 and state().counts[state().level+1]==1,'legacy result import invented a hint count or lost the solved game')
assert(files[base..'crossplay-1.dat']==old1 and files[base..'crossplay-2.dat']==old2,'first candidate journal changed during migration')
on_exit();collectgarbage('collect');local memory=collectgarbage('count')
for i=1,12 do
  app=dofile('main.lua');on_init();on_draw();on_exit();collectgarbage('collect')
end
assert(collectgarbage('count')<memory+8,'repeated reload retained app state')
print('production input, notes, undo, hints, full game, resume, cancel, clock, reload passed',frames)
