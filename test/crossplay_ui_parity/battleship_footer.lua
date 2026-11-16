local M=dofile('core.lua');local F=dofile('graphics.lua');local B=dofile('board.lua')
local w,h=smudge.get_bounds();F.init();local g=M.new(123456)
for row=1,5 do assert(M.action(g,1,row))end
local filled=false;local rect=smudge.rect
smudge.rect=function(x,y,width,height,fill,color,...)
 if x==16 and y==h-68 and width==w-32 and height==52 and fill and color then filled=true end
 return rect(x,y,width,height,fill,color,...)
end
B.draw(M,g,1,1,nil,F,w,h)
assert(filled,'Native Battleship status band has square filled corners')
print('Native square status band passed')
