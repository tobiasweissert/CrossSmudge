local a=T.open();T.shot('sudoku');T.tap(a.layout().difficulty);
assert(a.layout().menu[1]==nil,'native difficulty must cycle in place, not open a popup')
T.tap(a.layout().difficulty);T.tap(a.layout().difficulty);T.tap(a.layout().difficulty);T.tap(a.layout().howto)
for n=1,5 do T.shot('sudoku-help'..n);T.button('confirm')end
T.tap(a.layout().play)
for i=1,10000 do if not a.state().job then break end;on_update()end
assert(a.state().given,'no generated puzzle');T.shot('sudoku-board')
local g=a.state();local editable
for i=1,81 do if g.given[i]==0 then editable=i;break end end
local function cell(i,hold)local l=a.layout();if hold then on_long_press(l.x+(i-1)%9*l.size+l.size//2,l.y+(i-1)//9*l.size+l.size//2)else on_touch(l.x+(i-1)%9*l.size+l.size//2,l.y+(i-1)//9*l.size+l.size//2,'tap')end;on_draw()end
T.tap(a.layout().keys[g.solution[editable]]);cell(editable,true);assert(g.value[editable]==0 and g.notes[editable]~=0);T.shot('sudoku-pencil');T.tap(a.layout().buttons[3]);assert(g.notes[editable]==0)
local wrong;for j=(editable-1)//9*9+1,(editable-1)//9*9+9 do if g.given[j]~=0 then wrong=g.given[j];break end end
assert(wrong and wrong~=g.solution[editable]);T.tap(a.layout().keys[wrong]);cell(editable,false);T.shot('sudoku-conflict');T.tap(a.layout().buttons[3]);assert(g.value[editable]==0)

local before=table.concat(a.state().value)..table.concat(a.state().notes,',');T.tap(a.layout().buttons[2]);assert(a.state().pendingHint and a.state().hintsUsed==0 and before==table.concat(a.state().value)..table.concat(a.state().notes,','));on_draw();assert(before==table.concat(a.state().value)..table.concat(a.state().notes,','));T.shot('sudoku-highlight');T.tap(a.layout().buttons[2]);assert(a.state().hintsUsed==1);T.shot('sudoku-revealed')
local g=a.state()
for i=1,81 do if g.given[i]==0 and g.value[i]~=g.solution[i]then
 T.tap(a.layout().keys[g.solution[i]]);local l=a.layout();on_touch(l.x+(i-1)%9*l.size+l.size//2,l.y+(i-1)//9*l.size+l.size//2,'tap');on_draw()
end end
assert(g.solved and g.counts[g.level+1]==1 and g.hintsUsed==1);T.shot('sudoku-result');T.tap(a.layout().grid);T.button('back');T.shot('sudoku-record');on_exit()
