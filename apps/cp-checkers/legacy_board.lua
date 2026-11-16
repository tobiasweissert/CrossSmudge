-- Original CheckersScreens.cpp board, pieces, forced-capture marks and trays.
local B={}
function B.layout(w,h)
 local size=math.min(56,(w-32)//8,(h-250)//8);local x=(w-size*8)//2
 if w>h then size=math.min(44,(h-130)//8);x=24 end
 return {x=x,y=104,cell=size,w=w,h=h}
end
local function disc(F,x,y,r,piece)
 local filled=piece%2==0;F.ring(x,y,r,filled,3)
 if piece>=3 then smudge.circle(x,y,math.max(3,r//2),true,not filled);smudge.circle(x,y,math.max(1,r//2-3),true,filled)end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h);local side=l.cell*8
 if g.status>0 then
  local counts={0,0};for _,piece in ipairs(g.b)do if piece>0 then local seat=piece%2==1 and 1 or 2;counts[seat]=counts[seat]+1 end end
  F.header(g.status==3 and 'A DRAW' or mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WIN' or 'THEY WIN')
  F.center(counts[1]..' - '..counts[2],16,169,w-32,70,30)
  if g.status==3 then F.center('FORTY MOVES EACH',16,249,w-32,36,20);F.center('WITH NOTHING TAKEN.',16,285,w-32,36,20)end
  l.again={x=16,y=h-132,w=w-32,h=52};l.done={x=16,y=h-68,w=w-32,h=52};F.pill('PLAY AGAIN',l.again,true);F.pill('DONE',l.done,true);return l
 end
 smudge.rect(l.x-9,l.y-9,side+18,side+18,true,true);smudge.rect(l.x,l.y,side,side,true,false)
 local counts={0,0};local moves=M.moves(g);local must=false
 for _,move in ipairs(moves)do if #move>2 then must=true end end
 for i=1,64 do
  local r,c=(i-1)//8,(i-1)%8;local x,y=l.x+c*l.cell,l.y+r*l.cell;local p=g.b[i]
  if (r+c)%2==1 then smudge.rect_dither(x,y,l.cell,l.cell,false)end
  if p~=0 then disc(F,x+l.cell//2,y+l.cell//2,math.min(23,l.cell//2-5),p);counts[p%2==1 and 1 or 2]=counts[p%2==1 and 1 or 2]+1 end
 end
 local live=g.status==0 and (mode==2 or g.turn==1)
 if live then
  for _,move in ipairs(moves)do local from,to=move:byte(1,2)
   if g.selected==0 then local x,y=l.x+(from-1)%8*l.cell,l.y+(from-1)//8*l.cell;F.bracket(x,y,l.cell,l.cell,math.min(14,l.cell//3),3)
   elseif from==g.selected then
    smudge.circle(l.x+(to-1)%8*l.cell+l.cell//2,l.y+(to-1)//8*l.cell+l.cell//2,math.min(9,l.cell//5),true,true)
    for n=3,#move do local at=move:byte(n);F.bracket(l.x+(at-1)%8*l.cell,l.y+(at-1)//8*l.cell,l.cell,l.cell,math.min(18,l.cell//3),4)end
   end
  end
 end
 if g.selected>0 then local i=g.selected;smudge.rect(l.x+(i-1)%8*l.cell-3,l.y+(i-1)//8*l.cell-3,l.cell+6,l.cell+6,false,4,true)
 elseif live then smudge.rect(l.x+(cursor-1)%8*l.cell+1,l.y+(cursor-1)//8*l.cell+1,l.cell-2,l.cell-2,false,1,true)end
 local tx,ty=w>h and side+64 or l.x,w>h and 160 or l.y+side+25
 for row=0,1 do local seat=row==0 and 2 or 1;local cy=ty+row*34
  F.text(row==0 and 'THEM' or 'YOU',tx,cy-7,10)
  for n=1,counts[seat]do F.ring(tx+83+(n-1)*(w>h and 20 or 28),cy+7,w>h and 7 or 11,seat==2,2)end
 end
 local text=g.status==3 and 'DRAW' or g.status>0 and (mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WON' or 'THEY WON') or mode==2 and (g.turn==1 and 'LIGHT TO MOVE' or 'DARK TO MOVE') or live and (must and 'YOU MUST TAKE' or 'YOUR MOVE') or 'THEIR MOVE'
 F.pill(text,{x=w>h and side+64 or 16,y=h-68,w=w>h and w-side-88 or w-32,h=52},true)
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 if x>=l.x and x<l.x+l.cell*8 and y>=l.y and y<l.y+l.cell*8 then return 'cell',(y-l.y)//l.cell*8+(x-l.x)//l.cell+1 end
end
B.lesson=dofile('lesson.lua')
function B.preview(M,g,F,area)
 local cell=math.min(34,(area.h-36)//8,area.w//8);local side=cell*8;local x=area.x+(area.w-side)//2;local y=area.y+(area.h-side-30)//2
 for row=0,7 do for col=0,7 do local xx,yy=x+col*cell,y+row*cell;if(row+col)%2==1 then smudge.rect_dither(xx,yy,cell,cell,false)end;local p=g.b[row*8+col+1];if p>0 then disc(F,xx+cell//2,yy+cell//2,math.max(3,cell*23//56),p)end end end
 smudge.rect(x,y,side,side,false,2,true);F.center(g.status>0 and 'FINISHED GAME' or 'CURRENT POSITION',area.x,y+side+12,area.w,20,10)
end
return B
