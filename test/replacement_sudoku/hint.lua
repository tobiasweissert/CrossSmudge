-- Production callbacks in the exact int32/float32 bounded VM. No hint setters.
smudge.get_device=function() return 'sticky' end
local files,now={},1000
local assetRead=smudge.read_file
smudge.read_file=function(path,cap,offset)
 if path:match('^art/') then return assetRead(path,cap,offset) end
 local s=files[path];return s and s:sub((offset or 0)+1,(offset or 0)+cap)
end
smudge.write_file=function(path,s) files[path]=s;return true end
smudge.millis=function()return now end
local app=dofile('main.lua')
local function g()return app.state()end
local function draw()on_draw()end
local function tap(b) now=now+100;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');draw()end
local function hint()tap(app.layout().buttons[2])end
local function key(d)tap(app.layout().keys[d])end
local function cell(i)
 local l=app.layout();now=now+100;on_touch(l.x+(i-1)%9*l.size+l.size//2,l.y+(i-1)//9*l.size+l.size//2,'tap');draw()
end
local function snapshot()return table.concat(g().value)..'/'..table.concat(g().notes,',')end
local cycle=0
local function arm()
 cycle=cycle+1;print('hint cycle',cycle)
 local before=snapshot();local count=g().hintsUsed;local hinted=g().hinted
 hint()
 assert(snapshot()==before,'first Hint revealed a number or changed candidates')
 assert(g().hintsUsed==count and g().hinted==hinted,'highlight counted as assistance')
 assert(g().pendingHint and g().given[g().pendingHint]==0 and g().value[g().pendingHint]==0,'first Hint has no editable target: '..g().notice)
 local target=g().pendingHint
 local bounds=smudge.get_bounds;local w,h=bounds()
 smudge.get_bounds=function()return h,w end;draw();smudge.get_bounds=bounds;draw()
 assert(g().pendingHint==target and snapshot()==before,'orientation revealed or invalidated hint')
 draw();draw();on_update();assert(g().pendingHint==target and snapshot()==before,'redraw/update changed hint')
 return target
end
on_init();draw();tap(app.layout().play)
while g().job do on_update()end;draw()
local target=arm();hint()
assert(g().value[target]==g().solution[target] and g().hintsUsed==1 and g().hinted,'second Hint did not reveal exactly one digit')
assert(not g().pendingHint,'reveal left a stale target')
-- Undo invalidates the pending target even if no further cell is changed.
tap(app.layout().buttons[3]);target=arm();tap(app.layout().buttons[3]);assert(not g().pendingHint)
arm();on_button('right',true);draw();assert(not g().pendingHint,'button navigation kept stale hint');assert(g().notice~='Hint again reveals digit','navigation kept stale hint instruction')
target=arm();local d=g().solution[target];key(d);assert(not g().pendingHint,'keypad selection kept stale hint')
cell(target);assert(g().value[target]==d and g().hintsUsed==1,'manual solve counted a reveal')
target=arm();on_button('confirm',true);draw();assert(not g().pendingHint,'physical entry kept stale hint');tap(app.layout().buttons[3])
-- Exit/reload clears transient highlight and preserves saved state/statistics.
arm();local before=snapshot();on_exit();assert(not g().pendingHint)
app=dofile('main.lua');on_init();draw();assert(not g().pendingHint and snapshot()==before and g().hintsUsed==1,'reload restored stale target or lost save')
-- Menu and new puzzle cancel a pending target.
arm();on_button('back',true);draw();assert(not g().pendingHint)
tap(app.layout().play);arm();on_button('back',true);draw()
tap(app.layout().difficulty);assert(#app.layout().menu==0);tap(app.layout().play)
assert(not g().pendingHint,'replacement puzzle kept stale hint')
on_button('back',true);draw()
-- Fresh sessions: reach the last cell exclusively through keypad/board callbacks.
local function lastCell()
 on_exit();files={};app=dofile('main.lua');on_init();draw();tap(app.layout().play)
 while g().job do on_update()end;draw()
 local last
 for i=1,81 do if g().given[i]==0 then last=i end end
 for i=1,81 do if g().given[i]==0 and i~=last then key(g().solution[i]);cell(i)end end
 assert(not g().solved and g().hintsUsed==0)
 return last
end
local last=lastCell();assert(arm()==last);key(g().solution[last]);cell(last)
assert(g().solved and not g().hinted and g().hintsUsed==0 and g().counts[g().level+1]==1 and g().best[g().level+1]>=0,'manual finish after preview counted as assisted')
on_update();assert(g().counts[g().level+1]==1,'duplicate completion')
last=lastCell();assert(arm()==last);assert(not g().solved and not g().recorded,'preview completed puzzle')
hint();assert(g().solved and g().hintsUsed==1 and g().counts[g().level+1]==1 and g().best[g().level+1]==-1,'last-cell reveal result incorrect')
on_update();on_exit();app=dofile('main.lua');on_init();draw()
assert(g().solved and g().counts[g().level+1]==1 and g().hintsUsed==1,'assisted completion duplicated on reload')
-- A failed write preserves earlier journal data and never serializes pending state.
lastCell();on_update();local saved=snapshot();arm();hint()
local write=smudge.write_file;smudge.write_file=function()return false end
on_update();assert(g().notice:find('Save failed'),'write failure not surfaced')
on_exit();smudge.write_file=write;app=dofile('main.lua');on_init();draw()
assert(snapshot()==saved and not g().pendingHint and g().hintsUsed==0,'failed write lost previous journal or persisted a pending target')
print('two-stage Hint callbacks, orientation, manual/assisted completion, buttons, undo, menu, replacement, failed writes, exit/reload passed')
