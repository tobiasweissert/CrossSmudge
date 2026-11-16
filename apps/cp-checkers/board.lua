local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
if not ok or device~='sticky'then return dofile('legacy_board.lua')end
-- Original CheckersScreens.cpp board, pieces, forced-capture marks and trays.
local B={}
function B.layout(w,h)
 local size=math.min(56,(w-32)//8,(h-250)//8);local x=(w-size*8)//2
 if w>h then size=math.min(44,(h-130)//8);x=24 end
 return {x=x,y=104,cell=size,w=w,h=h}
end
local function disc(F,x,y,r,piece)
 local filled=piece%2==0;F.ring(x,y,r,filled,3)
 if piece>=3 then F.disc(x,y,12,not filled);F.disc(x,y,9,filled)end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h);local side=l.cell*8
 if g.status>0 then
  local counts={0,0};for _,piece in ipairs(g.b)do if piece>0 then local seat=piece%2==1 and 1 or 2;counts[seat]=counts[seat]+1 end end
  F.header(g.status==3 and 'A DRAW' or mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WIN' or 'THEY WIN')
  F.nativeText(counts[1]..' - '..counts[2],16,169,w-32,70,30)
  if g.status==3 then F.nativeText('FORTY MOVES EACH WITH NOTHING TAKEN.',16,249,w-32,60,20)end
  l.again={x=16,y=h-132,w=w-32,h=52};l.done={x=16,y=h-68,w=w-32,h=52};F.status('PLAY AGAIN',l.again);F.status('DONE',l.done);return l
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
    F.disc(l.x+(to-1)%8*l.cell+l.cell//2,l.y+(to-1)//8*l.cell+l.cell//2,math.min(9,l.cell//5),true)
    for n=3,#move do local at=move:byte(n);F.bracket(l.x+(at-1)%8*l.cell,l.y+(at-1)//8*l.cell,l.cell,l.cell,math.min(18,l.cell//3),4)end
   end
  end
 end
 if g.selected>0 then local i=g.selected;smudge.rect(l.x+(i-1)%8*l.cell-4,l.y+(i-1)//8*l.cell-4,l.cell+8,l.cell+8,false,4,true)
 end
 local tx,ty=w>h and side+64 or l.x,w>h and 160 or l.y+side+29
 for row=0,1 do local seat=row==0 and 2 or 1;local cy=ty+row*34
  F.text(row==0 and 'THEM' or 'YOU',tx,cy,10)
  for n=1,counts[seat]do F.ring(tx+83+(n-1)*(w>h and 20 or 28),cy+7,w>h and 7 or 11,seat==2,2)end
 end
 local text=g.status==3 and 'DRAW' or g.status>0 and (mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WON' or 'THEY WON') or mode==2 and (g.turn==1 and 'LIGHT TO MOVE' or 'DARK TO MOVE') or live and (must and 'YOU MUST TAKE' or 'YOUR MOVE') or 'THEIR MOVE'
 F.status(text,{x=w>h and side+64 or 16,y=h-68,w=w>h and w-side-88 or w-32,h=52},true)
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 if x>=l.x and x<l.x+l.cell*8 and y>=l.y and y<l.y+l.cell*8 then return 'cell',(y-l.y)//l.cell*8+(x-l.x)//l.cell+1 end
end
-- Original CheckersScreens.cpp men/crowns, compulsory jump and king diagrams.
local titles={'MEN AND CROWNS','TAKING','KINGS'}
local lines={
 'MEN STEP DIAGONALLY FORWARD. REACH THE FAR ROW AND A MAN IS CROWNED.',
 'IF YOU CAN TAKE, YOU MUST. A CHAIN OF JUMPS IS ONE MOVE.',
 'A CROWNED PIECE MOVES BOTH WAYS. NO MOVES LEFT MEANS YOU HAVE LOST.'}
local function lesson(page,F,w,h)
 page=math.max(1,math.min(3,page));F.header('HOW TO PLAY',page..' OF 3')
 F.nativeText(titles[page],16,119,w-32,46,30,nil,nil,true)
 local cell=h<600 and 50 or 88;local x=(w-cell*3)//2;local y=h<600 and 190 or 235
 for r=0,2 do for c=0,2 do if (r+c)%2==(page==1 and 1 or 0)then smudge.rect_dither(x+c*cell,y+r*cell,cell,cell,false)end;smudge.rect(x+c*cell,y+r*cell,cell,cell,false,1,true)end end
 local function piece(c,r,dark,king)
  local cx,cy=x+c*cell+cell//2,y+r*cell+cell//2;local radius=cell*23//56
  F.ring(cx,cy,radius,dark,3)
  if king then F.disc(cx,cy,radius//2,not dark);F.disc(cx,cy,radius//2-3,dark)end
 end
 local function dot(c,r)F.disc(x+c*cell+cell//2,y+r*cell+cell//2,cell*9//56+3,true)end
 if page==1 then piece(1,2,false,false);dot(0,1);dot(2,1);piece(1,0,false,true)
 elseif page==2 then piece(0,2,false,false);piece(1,1,true,false);F.bracket(x+cell,y+cell,cell,cell,cell//3,4);dot(2,0)
 else piece(1,1,false,true);dot(0,0);dot(2,0);dot(0,2);dot(2,2)end
 F.nativeText(lines[page],16,h<600 and 350 or 524,w-32,h<600 and 90 or 200,h<600 and 10 or 20)
 F.nativeText(page==3 and 'TAP TO FINISH' or 'TAP TO CONTINUE',16,h-64,w-32,22,10)
 local left=(w-62)//2;for i=1,3 do smudge.rounded_rect(left+(i-1)*24,h-30,14,14,7,i==page,true,1)end
 return {page=page,pages=3,boxes={},whole=true}
end

B.lesson=lesson
function B.preview(M,g,F,area,Q)
 local cell=math.min(34,(area.h-(area.h<200 and 48 or 36))//8,area.w//8);local side=cell*8;local x=area.x+(area.w-side)//2;local y=area.y+(area.h>side+36 and (area.h-side-36)//2 or 12)
 for row=0,7 do for col=0,7 do local xx,yy=x+col*cell,y+row*cell;if(row+col)%2==1 then smudge.rect_dither(xx,yy,cell,cell,false)end;smudge.rect(xx,yy,cell,cell,false,1,true);local p=g.b[row*8+col+1];if p>0 then local cx,cy,r=xx+cell//2,yy+cell//2,math.max(3,cell*23//56);local dark=p%2==0;F.ring(cx,cy,r,dark,2);if p>=3 then F.disc(cx,cy,r//2,not dark);F.disc(cx,cy,r//2-3,dark)end end end end
 smudge.rect(x-3,y-3,side+6,side+6,false,3,true)
 local counts={0,0};for _,p in ipairs(g.b)do if p>0 then local seat=p%2==1 and 1 or 2;counts[seat]=counts[seat]+1 end end
 local margin=counts[1]-counts[2];local caption=g.status==3 and 'LAST GAME: A DRAW' or g.status==1 and ('LAST GAME: WON'..(margin>0 and ' BY '..margin or ''))or('LAST GAME: LOST'..(margin<0 and ' BY '..(-margin)or''))
 F.nativeText(caption,area.x,y+side+12,area.w,24,10)
end
return B
