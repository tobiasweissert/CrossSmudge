local C=dofile('cp_core.lua')
local g=C.deal(42,true)
assert(not C.canMove(g,-1,6,1));assert(not C.canMove(g,1,13,1));assert(not C.canMove(g,0,6,1));assert(not C.canMove(g,6,6,1));assert(not C.canMove(g,6,7,0))
local original=C.save(g)
for i=1,8 do assert(C.drawFromStock(g)) end
assert(#g.piles[0]==0 and #g.piles[1]==24)
assert(C.drawFromStock(g));assert(#g.piles[0]==24 and #g.piles[1]==0)
for i=1,9 do assert(C.undo(g)) end
assert(C.save(g)==original,'stock cycle/recycle/undo changed deal')
for i=1,120 do C.drawFromStock(g) end
local undos=0;while C.undo(g) do undos=undos+1;assert(undos<=96) end
assert(undos==96,'undo retention differs from source')
-- Construct a reachable all-face-up endgame: ace through queen in foundations,
-- four kings in tableau. Auto finish must move exactly four cards and win.
g=C.deal(1,false)
for p=0,12 do g.piles[p]={} end
for suit=0,3 do
 for rank=0,11 do g.piles[2+suit][rank+1]=(suit<<4)|rank|128 end
 g.piles[6+suit]={ (suit<<4)|12|128 }
end
g.moves=0
assert(C.canAutoFinish(g));assert(C.autoPlay(g)==4);assert(C.won(g));assert(not C.canAutoFinish(g));assert(C.autoPlay(g)==0)
local saved=C.save(g);assert(C.restore(saved) and C.won(C.restore(saved)))
assert(C.undo(g) and not C.won(g))
-- Every truncated serialization must be rejected; invalid input must not mutate
-- any independently held live game.
local fresh=C.deal(7,false);local text=C.save(fresh)
for i=0,#text-1 do assert(not C.restore(text:sub(1,i)),'truncated state accepted '..i) end
for _,bad in ipairs({'',string.rep('x',4096),text..'extra'}) do assert(not C.restore(bad)) end
assert(C.save(fresh)==text)
print('Solitaire stock/undo/full-game/corrupt-save checks passed')
