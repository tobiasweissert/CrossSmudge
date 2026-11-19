-- Original CheckersScreens.cpp men/crowns, compulsory jump and king diagrams.
local titles={'MEN AND CROWNS','TAKING','KINGS'}
local lines={
 'MEN STEP DIAGONALLY FORWARD. REACH THE FAR ROW AND A MAN IS CROWNED.',
 'IF YOU CAN TAKE, YOU MUST. A CHAIN OF JUMPS IS ONE MOVE.',
 'A CROWNED PIECE MOVES BOTH WAYS. NO MOVES LEFT MEANS YOU HAVE LOST.'}
return function(page,F,w,h)
 page=math.max(1,math.min(3,page));F.header('HOW TO PLAY',page..' OF 3')
 F.center(titles[page],16,119,w-32,46,30)
 local cell=h<600 and 50 or 88;local x=(w-cell*3)//2;local y=h<600 and 190 or 235
 for r=0,2 do for c=0,2 do if (r+c)%2==(page==1 and 1 or 0)then smudge.rect_dither(x+c*cell,y+r*cell,cell,cell,false)end;smudge.rect(x+c*cell,y+r*cell,cell,cell,false,1,true)end end
 local function piece(c,r,dark,king)
  local cx,cy=x+c*cell+cell//2,y+r*cell+cell//2;local radius=cell*23//56
  F.ring(cx,cy,radius,dark,3)
  if king then smudge.circle(cx,cy,radius//2,true,not dark);smudge.circle(cx,cy,radius//2-3,true,dark)end
 end
 local function dot(c,r)smudge.circle(x+c*cell+cell//2,y+r*cell+cell//2,cell*9//56+3,true,true)end
 if page==1 then piece(1,2,false,false);dot(0,1);dot(2,1);piece(1,0,false,true)
 elseif page==2 then piece(0,2,false,false);piece(1,1,true,false);F.bracket(x+cell,y+cell,cell,cell,cell//3,4);dot(2,0)
 else piece(1,1,false,true);dot(0,0);dot(2,0);dot(0,2);dot(2,2)end
 F.lessonText(lines[page],16,h<600 and 350 or 524,w-32,h<600 and 10 or 20)
 F.center(page==3 and 'TAP TO FINISH' or 'TAP TO CONTINUE',16,h-64,w-32,22,10)
 local left=(w-62)//2;for i=1,3 do F.ring(left+(i-1)*24+7,h-23,7,i==page,1)end
 return {page=page,pages=3,boxes={},whole=true}
end
