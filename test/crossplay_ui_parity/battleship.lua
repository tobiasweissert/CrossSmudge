-- Execute shipped board/fonts and assert the native board's single footer.
-- Original bshipui::buildBoardChrome takes one full-width 52px bottom band.
local M=dofile('core.lua');local F=dofile('graphics.lua');local B=dofile('board.lua')
local w,h=smudge.get_bounds();F.init();local g=M.new(123456)
for row=1,5 do assert(M.action(g,1,row))end
local l=B.layout(w,h,g)
assert(l.view.x==16 and l.view.y==h-68 and l.view.w==w-32 and l.view.h==52,
 'Battleship status/fire must occupy the original full-width footer')
smudge.clear();F.header(M.title);l=B.draw(M,g,1,1,nil,F,w,h)
assert(B.hit(l,l.view.x+8,l.view.y+8,F)==nil,'unarmed status fired')
local kind,at=B.hit(l,l.x+l.cell//2,l.y+l.cell//2,F)
assert(kind=='aim' and at==1,'drawn target did not aim')
kind,at=B.hit(l,l.view.x+8,l.view.y+8,F)
assert(kind=='cell'and at==1,'full-width status did not fire aimed cell')
assert(M.fire(g,at));local own=M.choose(g);assert(M.fire(g,own))
local before=M.encode(g)
B.draw(M,g,1,1,nil,F,w,h)
assert(M.encode(g)==before,'drawing changed game state')
print('Native full-width Battleship status/aim/fire geometry passed')
