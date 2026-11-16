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
   smudge.rect(xx,yy,l.cell,l.cell,true,same>=2);smudge.rect(xx,yy,l.cell,l.cell,false,2,true)
   F.die(xx,yy,l.cell,value,same>=2)
  end
  local total=M.column_score(g,seat,col+1)
  if total>0 then F.center(tostring(total),x+col*(l.cell+l.gap),y+l.side+4,l.cell,28,20)end
  if seat==l.activeSeat and l.canAct and g.status==0 and g.b[base+3]==0 then F.bracket(x+col*(l.cell+l.gap)-4,y-4,l.cell+8,l.side+8,12,3)end
 end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h)
 if g.status>0 then
  F.header(g.status==3 and 'A DRAW' or mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WIN' or 'THEY WIN')
  F.center(M.score(g,1)..' - '..M.score(g,2),16,179,w-32,80,30)
  l.again={x=16,y=h-152,w=w-32,h=62};l.done={x=16,y=h-78,w=w-32,h=62};F.pill('PLAY AGAIN',l.again,true);F.pill('DONE',l.done,true);return l
 end
 l.activeSeat=mode==2 and g.turn or 1;l.canAct=mode==2 or g.turn==1
 grid(M,g,2,l.theirs,l.otherX or l.x,l,F);grid(M,g,1,l.yours,l.x,l,F)
 local dx=w>h and (w-l.cell)//2 or l.x+l.cell+l.gap
 smudge.rect(dx,l.die,l.cell,l.cell,true,false);smudge.rect(dx,l.die,l.cell,l.cell,false,4,true)
 if g.die~=0 then F.die(dx,l.die,l.cell,g.die,false)end
 local totals={{'THEM',M.score(g,2)},{'YOU',M.score(g,1)}}
 for n,v in ipairs(totals)do
  local tx=w>h and (w-120)//2 or n==1 and 16 or w-136;local ty=w>h and (n==1 and l.die-80 or l.die+l.cell+12)or l.die
  F.center(v[1],tx,ty,120,24,10);F.center(tostring(v[2]),tx,ty+25,120,44,30)
 end
 local live=mode==2 or g.turn==1
 local label=g.status==3 and 'DRAW' or g.status>0 and (mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WON' or 'THEY WON') or mode==2 and 'PLAYER '..g.turn..' ROLL' or live and 'YOUR ROLL' or 'THEIR ROLL'
 F.pill(label,{x=16,y=h-68,w=w-32,h=52},true)
 l.activeSeat=mode==2 and g.turn or 1
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 local left=l.activeSeat==2 and (l.otherX or l.x)or l.x;local top=l.activeSeat==2 and l.theirs or l.yours
 if y<top or y>=top+l.side then return end
 for col=0,2 do local xx=left+col*(l.cell+l.gap);if x>=xx and x<xx+l.cell then return 'cell',col+1 end end
end
B.lesson=dofile('lesson.lua')
function B.preview(M,g,F,area)
 local cell=math.min(44,(area.h-64)//6,(area.w-8)//3);local side=3*cell+8;local x=area.x+(area.w-side)//2;local top=area.y+(area.h-2*side-48)//2
 for seat=1,2 do local y=top+(seat==1 and side+48 or 0)
  for col=0,2 do for row=0,2 do local xx,yy=x+col*(cell+4),y+(2-row)*(cell+4);smudge.rect(xx,yy,cell,cell,false,2,true);F.die(xx,yy,cell,g.b[(seat-1)*9+col*3+row+1],false)end end
 end
 F.center(M.score(g,1)..' - '..M.score(g,2),area.x,top+side+12,area.w,24,20)
end
return B
