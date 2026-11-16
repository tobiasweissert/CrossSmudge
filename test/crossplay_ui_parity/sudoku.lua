-- Actual first/second Hint callbacks and rendered pixels; identity comes from
-- the compiled production binding installed by the routine capture runner.
local read,write=smudge.read_file,smudge.write_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
tap(app.layout().play)
local ticks=0
while app.state().job do on_update();ticks=ticks+1;assert(ticks<10000)end
on_draw();capture('sudoku-before.pgm')
local g=app.state();local before=table.concat(g.value)..'/'..table.concat(g.notes,',')
tap(app.layout().buttons[2]);capture('sudoku-highlight.pgm')
assert(before==table.concat(g.value)..'/'..table.concat(g.notes,','),'preview changed board/candidates')
assert(g.pendingHint and g.hintsUsed==0 and not g.hinted,'preview counted assistance')
local target=g.pendingHint;local l=app.layout()
assert(write('sudoku-target.txt',table.concat({target,l.x+(target-1)%9*l.size,l.y+(target-1)//9*l.size,l.size},' '),false))
on_draw();assert(g.pendingHint==target and g.value[target]==0,'redraw revealed digit')
tap(app.layout().buttons[2]);capture('sudoku-revealed.pgm')
assert(g.value[target]==g.solution[target] and g.hintsUsed==1 and g.hinted and not g.pendingHint,'second explicit Hint failed')
on_exit()
print('Production Sudoku highlight/reveal pixel fixture passed')
