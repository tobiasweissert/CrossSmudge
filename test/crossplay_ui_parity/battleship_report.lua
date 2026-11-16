-- Observe text entering shipped glyph renderer from production board state.
local M=dofile('core.lua');local F=dofile('graphics.lua');local B=dofile('board.lua')
local w,h=smudge.get_bounds();F.init();local g=M.new(123456)
for row=1,5 do assert(M.action(g,1,row))end
local original=F.text;local seen={}
F.text=function(text,...)seen[#seen+1]=text;return original(text,...)end
local function expect(label)
 seen={};B.draw(M,g,1,1,nil,F,w,h)
 for _,text in ipairs(seen)do if text==label then return end end
 error('Native Battleship report missing: '..label)
end
expect('YOUR MOVE')
local miss
for i=1,100 do if g.b:sub(i,i)=='0'then miss=i;break end end
assert(M.fire(g,miss));expect(string.char(65+(miss-1)%10)..((miss-1)//10+1)..': MISS')
local ownMiss
for i=1,100 do if g.a:sub(i,i)=='0'then ownMiss=i;break end end
assert(M.fire(g,ownMiss));expect('THEY MISSED AT '..string.char(65+(ownMiss-1)%10)..((ownMiss-1)//10+1))
print('Actual state native last-shot narration passed')
