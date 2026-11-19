-- Original KnucklebonesScreens.cpp facing, multiplier and destruction pages.
local lines={
 'TWO BOARDS FACING. ROLL A DIE, DROP IT IN ONE OF YOUR THREE COLUMNS.',
 'MATCHING DICE MULTIPLY. THREE 4S SCORE 36, NOT 12.',
 'YOUR 5 DESTROYS EVERY 5 IN THEIR FACING COLUMN.'}
return function(page,F,w,h)
 page=math.max(1,math.min(3,page));F.header('HOW TO PLAY',page..' OF 3')
 F.lessonText(lines[page],16,119,w-32,h<600 and 10 or 20)
 local cell=h<600 and 28 or (page==1 and 52 or 58);local side=cell*3+8;local left=(w-side)//2;local top=h<600 and 175 or 259
 local function grid(cells,y,bracket)
  for col=0,2 do for row=0,2 do local xx,yy=left+col*(cell+4),y+(2-row)*(cell+4)
   smudge.rect(xx,yy,cell,cell,false,2,true);F.die(xx,yy,cell,cells[col+1][row+1],false)
  end;if bracket then F.bracket(left+col*(cell+4),y,cell,side,16,4)end end
 end
 if page==1 then
  grid({{3,0,0},{0,0,0},{6,2,0}},top,false)
  local dy=top+side+(h<600 and 8 or 14);local dx=(w-cell)//2;smudge.rect(dx,dy,cell,cell,false,4,true);F.die(dx,dy,cell,4,false)
  grid({{0,0,0},{5,0,0},{0,0,0}},dy+cell+14,true)
 elseif page==2 then
  grid({{4,4,4},{0,0,0},{0,0,0}},top,false);grid({{4,0,0},{4,0,0},{4,0,0}},top+side+(h<600 and 20 or 40),false)
  F.center('36',left+side+12,top+(side-44)//2,w-16-left-side-12,44,30)
  F.center('12',left+side+12,top+side+(h<600 and 20 or 40)+(side-44)//2,w-16-left-side-12,44,30)
 else
  grid({{5,5,0},{0,0,0},{0,0,0}},top,false)
  local arrow=top+side+10;for n=0,4 do smudge.line(w//2-14+n,arrow+30,w//2+n,arrow+8,true);smudge.line(w//2+n,arrow+8,w//2+14+n,arrow+30,true)end
  grid({{5,0,0},{0,0,0},{0,0,0}},top+side+(h<600 and 34 or 60),true)
 end
 local next={x=16,y=h-68,w=w-32,h=52};F.pill(page==3 and 'GOT IT' or 'NEXT',next,true)
 return {page=page,pages=3,boxes={next}}
end
