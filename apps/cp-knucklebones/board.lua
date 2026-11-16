local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
if not ok or device~='sticky'then return dofile('legacy_board.lua')end
-- Original KnucklebonesScreens.cpp opposing grids and central rolled die.
local B={}
function B.layout(w,h)
 local cell=math.min(73,(h-350)//6);local gap=6;local side=cell*3+gap*2
 if w>h then cell=math.min(73,(h-210)//3);side=cell*3+gap*2 end
 local l={cell=cell,gap=gap,side=side,w=w,h=h}
 l.x=(w-side)//2;l.theirs=99;l.die=99+side+4+28+18;l.yours=l.die+cell+8
 if w>h then l.x=24;l.yours=130;l.theirs=130;l.otherX=w-side-24;l.die=130+(side-cell)//2 end
 return l
end
local function grid(M,g,seat,y,x,l,F)
 if seat==2 then smudge.rect_dither(x-4,y-4,l.side+8,l.side+8,false)end
 for col=0,2 do
  local base=(seat-1)*9+col*3
  for row=0,2 do
   local value=g.b[base+row+1];local yy=y+(seat==1 and 2-row or row)*(l.cell+l.gap);local xx=x+col*(l.cell+l.gap)
   local same=0;if value~=0 then for n=1,3 do if g.b[base+n]==value then same=same+1 end end end
   smudge.rect(xx,yy,l.cell,l.cell,true,same>=2);if same<2 then smudge.rect(xx,yy,l.cell,l.cell,false,2,true)end
   F.die(xx,yy,l.cell,value,same>=2)
  end
  if seat==l.activeSeat and l.canAct and g.status==0 and g.b[base+3]==0 then F.bracket(x+col*(l.cell+l.gap),y,l.cell,l.side,16,4)end
  local total=M.column_score(g,seat,col+1)
  if total>0 then F.nativeText(tostring(total),x+col*(l.cell+l.gap),y+l.side+4,l.cell,28,20,nil,nil,true)end

 end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h)
 if g.status>0 then
  F.header(g.status==3 and 'A DRAW' or mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WIN' or 'THEY WIN')
  F.nativeText(M.score(g,1)..' - '..M.score(g,2),16,179,w-32,80,30)
  l.again={x=16,y=h-152,w=w-32,h=62};l.done={x=16,y=h-78,w=w-32,h=62};F.status('PLAY AGAIN',l.again);F.status('DONE',l.done);return l
 end
 l.activeSeat=mode==2 and g.turn or 1;l.canAct=mode==2 or g.turn==1
 grid(M,g,2,l.theirs,l.otherX or l.x,l,F);grid(M,g,1,l.yours,l.x,l,F)
 local dx=w>h and (w-l.cell)//2 or l.x+l.cell+l.gap
 smudge.rect(dx,l.die,l.cell,l.cell,true,false);smudge.rect(dx,l.die,l.cell,l.cell,false,4,true)
 if g.die~=0 then F.die(dx,l.die,l.cell,g.die,false)end
 local dieLeft=(w-l.cell)//2;local zone=dieLeft-28;local right=dieLeft+l.cell+12
 if w<h then
  F.text('THEM',16,l.die+4,10);F.text(tostring(M.score(g,2)),16,l.die+28,30)
  F.text('YOU',w-16-F.inkWidth('YOU',10),l.die+4,10)
  local total=tostring(M.score(g,1));F.text(total,w-16-F.inkWidth(total,30),l.die+28,30)
 else
  F.center('THEM',340,130,120,24,10);F.center(tostring(M.score(g,2)),340,158,120,44,30)
  F.center('YOU',340,310,120,24,10);F.center(tostring(M.score(g,1)),340,338,120,44,30)
 end
 local live=mode==2 or g.turn==1
 local label=g.status==3 and 'DRAW' or g.status>0 and (mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WON' or 'THEY WON') or mode==2 and 'PLAYER '..g.turn..' ROLL' or live and 'YOUR ROLL' or 'THEIR ROLL'
 F.status(label,{x=0,y=h-68,w=w,h=52})
 l.activeSeat=mode==2 and g.turn or 1
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 local left=l.activeSeat==2 and (l.otherX or l.x)or l.x;local top=l.activeSeat==2 and l.theirs or l.yours
 if y<top or y>=top+l.side then return end
 for col=0,2 do local xx=left+col*(l.cell+l.gap);if x>=xx and x<xx+l.cell then return 'cell',col+1 end end
end
-- Original KnucklebonesScreens.cpp facing, multiplier and destruction pages.
local lines={
 'TWO BOARDS FACING. ROLL A DIE, DROP IT IN ONE OF YOUR THREE COLUMNS.',
 'MATCHING DICE MULTIPLY. THREE 4s SCORE 36, NOT 12.',
 'YOUR 5 DESTROYS EVERY 5 IN THEIR FACING COLUMN.'}
local function lesson(page,F,w,h)
 page=math.max(1,math.min(3,page));F.header('HOW TO PLAY',page..' OF 3')
 F.nativeText(lines[page],16,h<600 and 107 or 119,w-32,h<600 and 52 or 130,h<600 and 10 or 20)
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
  F.nativeText('36',left+side+12,top+(side-44)//2,w-16-left-side-12,44,30,nil,nil,true)
  F.nativeText('12',left+side+12,top+side+(h<600 and 20 or 40)+(side-44)//2,w-16-left-side-12,44,30,nil,nil,true)
 else
  grid({{5,5,0},{0,0,0},{0,0,0}},top,false)
  F.nativeText('^',16,top+3*cell+10,w-32,h<600 and 28 or 44,h<600 and 20 or 30,nil,nil,true)
  grid({{5,0,0},{0,0,0},{0,0,0}},top+3*cell+(h<600 and 50 or 60),true)
 end
 local next={x=16,y=h-68,w=w-32,h=52};F.status(page==3 and 'GOT IT' or 'NEXT',next)
 return {page=page,pages=3,boxes={next}}
end

B.lesson=lesson
function B.preview(M,g,F,area,Q)
 local cell=math.min(44,(area.h-64)//6,(area.w-8)//3);local side=3*cell+8;local x=area.x+(area.w-side)//2;local top=area.y+(area.h-2*side-48)//2
 for seat=1,2 do local y=top+(seat==1 and side+48 or 0)
  for col=0,2 do for row=0,2 do local xx,yy=x+col*(cell+4),y+(2-row)*(cell+4);smudge.rect(xx,yy,cell,cell,false,2,true);if cell>=36 then F.die(xx,yy,cell,g.b[(seat-1)*9+col*3+row+1],false)else
 local value=g.b[(seat-1)*9+col*3+row+1];local pip=math.max(1,cell//5);local step=math.max(1,cell//4)
 local function dot(dx,dy)smudge.rect(xx+2*step+dx*step-pip//2,yy+2*step+dy*step-pip//2,pip,pip,true,true)end
 if value%2==1 then dot(0,0)end;if value>=2 then dot(-1,-1);dot(1,1)end;if value>=4 then dot(-1,1);dot(1,-1)end;if value==6 then dot(-1,0);dot(1,0)end end end end
 end
 F.nativeText(Q.wins..' W   '..Q.losses..' L   '..Q.draws..' D',area.x,top+side+12,area.w,24,10)
end
return B
