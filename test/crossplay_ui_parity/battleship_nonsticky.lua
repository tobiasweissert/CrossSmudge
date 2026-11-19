-- UI changes belong to Sticky. Preserve existing larger-heap X4 Pro layout.
smudge.get_device=function()return 'x4pro'end
local B=dofile('board.lua');local g=dofile('core.lua').new(123456)
for row=1,5 do assert(dofile('core.lua').action(g,1,row))end
local w,h=smudge.get_bounds();local l=B.layout(w,h,g)
assert(l.view.x==(w+12)//2 and l.view.w==(w-44)//2,'non-Sticky Battleship footer changed')
local F=dofile('graphics.lua');F.init();smudge.clear();F.header('BATTLESHIP');B.draw(dofile('core.lua'),g,1,1,nil,F,w,h)
print('Existing non-Sticky larger-heap Battleship layout preserved')
