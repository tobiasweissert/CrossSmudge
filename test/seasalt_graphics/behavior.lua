local M=dofile('core.lua');local V=dofile('board.lua');V.init(false)
local g=M.new(12345);V.menu=false
local function draw()
 V.draw(M,g,1,'');assert(M.valid(g),'render changed validity')
 for _,tile in ipairs(V.tiles or{})do
  local place=g.place[tile.c]
  assert(place~=2 or g.phase>=2 or(g.revealed&2)~=0,'hidden opponent card drawn')
 end
 for _,c in ipairs(V.controls)do
  assert(c.box.w>0 and c.box.h>0,'empty touch target')
 end
end
draw();local before=M.encode(g)
-- Deck and pile targets execute their exact existing action index.
local picked
for _,c in ipairs(V.controls)do if c.op=='act'then picked=c;break end end
assert(picked);local b=picked.box;assert(V.touch(b.x+b.w//2,b.y+b.h//2)==picked.value);assert(M.encode(g)==before)
M.act(g,picked.value);assert(g.step==1);draw()
assert(#V.tiles==2)
local keep=V.controls
for _,c in ipairs(keep)do if c.op=='act'and c.value==V.tiles[1].index then picked=c;break end end
b=picked.box;local index=V.touch(b.x+b.w//2,b.y+b.h//2);assert(index);M.act(g,index);draw()
-- Original card faces are retained through every round and a whole match.
local states={};local frames=0
for i=1,3500 do
 draw();states[g.step]=true;frames=frames+1
 if g.phase==3 then break elseif g.phase==2 then M.act(g,1)
 else local move=M.ai(g);assert(move);assert(M.apply(g,move))end
end
assert(g.phase==3,'match did not complete');assert(states[0]and states[1]and states[2]and states[3])
-- Revealed hands are public, and tab switching never exposes an unrevealed one.
g=M.new(67890);for i=1,8 do assert(M.apply(g,M.ai(g)));if g.phase>=2 then break end end
while g.phase<2 and g.step~=0 and g.step~=3 do assert(M.apply(g,M.ai(g)))end
V.menu=false;V.tab=2;draw();for _,tile in ipairs(V.tiles)do assert(g.place[tile.c]==4 or(g.revealed&2)~=0 or g.phase>=2)end
V.menu=true;V.help=1
for page=1,6 do V.draw(M,g,1,'');local next
 for _,c in ipairs(V.controls)do if c.op=='lesson'then next=c;break end end
 assert(next);V.touch(next.box.x+2,next.box.y+2)
end
assert(V.help==0)
print('Sea Salt original masks / complete-match states / hit indices / private cards / six help pages',frames)
