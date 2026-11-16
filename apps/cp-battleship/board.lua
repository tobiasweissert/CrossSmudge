-- Original BattleshipActivity.cpp target marks, fleet readout and roster.
local B={aim=0};local lengths={5,4,3,3,2};local names={'CARRIER','BATTLESHIP','CRUISER','SUBMARINE','DESTROYER'}
function B.layout(w,h,g)
 local cell=math.min(44,(w-50)//10,(h-(g.phase==0 and 250 or 430))//10);local x=(w-cell*10)//2
 local l={cell=math.max(8,cell),x=x,y=160,w=w,h=h}
 if w>h then l.cell=math.min(28,(h-220)//10);l.x=24;l.y=119 end
 l.rotate={x=16,y=h-68,w=(w-44)//2,h=52};l.view={x=(w+12)//2,y=h-68,w=(w-44)//2,h=52}
 return l
end
local function sunk(board,shots,ship)
 local hit=0;for i=1,100 do if board:sub(i,i)==tostring(ship) and shots:sub(i,i)=='1' then hit=hit+1 end end
 return hit==lengths[ship]
end
local function grid(F,board,shots,x,y,cell,reveal,heavy)
 for i=1,100 do local xx=x+(i-1)%10*cell;local yy=y+(i-1)//10*cell;local shot=shots:sub(i,i)=='1'
  -- Never consult an unshot enemy cell for a visible mark.
  if shot or reveal then local ship=tonumber(board:sub(i,i))
   if reveal and ship>0 then smudge.rect_dither(xx+1,yy+1,cell-2,cell-2,false)end
   if shot then
    if not heavy then
     if ship>0 then smudge.rect(xx+1,yy+1,cell-2,cell-2,true,true)else smudge.circle(xx+cell//2,yy+cell//2,math.max(1,cell//6),true,true)end
    elseif ship==0 then F.ring(xx+cell//2,yy+cell//2,math.max(3,cell//6),false,2)
    elseif sunk(board,shots,ship)then smudge.rect(xx,yy,cell,cell,true,true)
    else smudge.rounded_rect(xx+cell//4,yy+cell//4,cell//2,cell//2,math.min(3,cell//6),true,true)end
   end
  end
 end
 for n=1,9 do smudge.rect(x+n*cell,y,1,cell*10,true,true);smudge.rect(x,y+n*cell,cell*10,1,true,true)end
 smudge.rect(x-(heavy and 4 or 1),y-(heavy and 4 or 1),cell*10+(heavy and 8 or 2),cell*10+(heavy and 8 or 2),false,heavy and 4 or 1,true)
 if heavy then F.bracket(x-9,y-9,cell*10+18,cell*10+18,24,4)end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h,g)
 local own=g.phase==0 or g.show==1
 if g.phase==0 then F.header('YOUR FLEET')end
 F.text(F.fit(M.message(g),w-32,10),16,125,10)
 grid(F,own and g.a or g.b,own and g.sa or g.sb,l.x,l.y,l.cell,own or g.status>0,true)
 if g.status==0 and (g.phase==0 or B.aim>0)then local at=g.phase==0 and cursor or B.aim;F.bracket(l.x+(at-1)%10*l.cell,l.y+(at-1)//10*l.cell,l.cell,l.cell,math.max(8,l.cell//3),3)end
 if g.phase==0 then
  local top=l.y+l.cell*10+21;local row=math.max(12,(h-80-top)//5)
  if w>h then top=150;row=32 end
  local rx=w>h and l.x+l.cell*10+32 or 16;local width=w-rx-16
  for ship,name in ipairs(names)do local y=top+(ship-1)*row;local chosen=ship==g.ship
   if chosen then smudge.rect_dither(rx,y,width,row-2,false);smudge.rect(rx,y,width,row-2,false,3,true)end
   F.text(name,rx+6,y+(row-13)//2,10)
   local pip=math.max(5,math.min(14,row-8));for n=1,lengths[ship]do local x=rx+width-(pip+3)*(lengths[ship]-n+1)
    smudge.rect_dither(x,y+(row-pip)//2,pip,pip,false);smudge.rect(x,y+(row-pip)//2,pip,pip,false,chosen and 3 or 1,true)
   end
  end
  F.pill(g.rotate==0 and 'ROTATE: HORIZONTAL' or 'ROTATE: VERTICAL',l.rotate,false,10)
  F.pill('MENU',l.view,false)
 else
  local small=16;local fx,fy=20,l.y+l.cell*10+21
  if w>h then fx=w-180;fy=119 end
  grid(F,g.a,g.sa,fx,fy,small,true,false)
  local rx,ry=w>h and l.x+l.cell*10+32 or fx+184,w>h and 150 or fy
  F.text('THEIR FLEET',rx,ry+6,10)
  for ship,name in ipairs(names)do local yy=ry+ship*26;local down=sunk(g.b,g.sb,ship)
   if down then smudge.rect(rx,yy,(w>h and fx-24 or w-16)-rx,24,true,true)end
   F.text(name,rx+6,yy+5,10,down)
  end
  F.pill(own and 'TARGET GRID' or 'YOUR FLEET',l.rotate,false)
  local label=g.status>0 and 'PLAY AGAIN' or g.turn~=1 and 'THEIR SHOT' or own and 'YOUR FLEET' or B.aim==0 and 'TAP A TARGET' or 'FIRE AT '..string.char(65+(B.aim-1)%10)..((B.aim-1)//10+1)
  F.pill(label,l.view,true)
 end
 l.own=own;l.phase=g.phase;l.canFire=g.turn==1 and g.status==0;l.finished=g.status>0;return l
end
function B.hit(l,x,y,F)
 if F.inside(l.rotate,x,y)then B.aim=0;return 'action',l.phase==0 and 2 or 3 end
 if l.phase==0 and F.inside(l.view,x,y)then return 'back' end
 if l.phase==1 and F.inside(l.view,x,y)then
  if l.finished then return 'new' end
  if not l.own and l.canFire and B.aim>0 then local aim=B.aim;B.aim=0;return 'cell',aim end
 end
 if x>=l.x and x<l.x+l.cell*10 and y>=l.y and y<l.y+l.cell*10 then
  local at=(y-l.y)//l.cell*10+(x-l.x)//l.cell+1
  if l.phase==0 then return 'cell',at elseif not l.own and l.canFire then B.aim=at;return 'aim',at end
 end
end
B.help={'PLACE FIVE SHIPS OF LENGTHS 5, 4, 3, 3 AND 2. CHOOSE EACH BOW.', 'TAP ROTATE BEFORE PLACING A SHIP. SHIPS MAY TOUCH BUT CANNOT OVERLAP.', 'TAP A TARGET TO AIM. THE FIRE BUTTON COMMITS YOUR SHOT.', 'THE SMALL FLEET SHOWS YOUR SHIPS. THE ROSTER MARKS SUNK ENEMY SHIPS.', 'TAP VIEW TO INSPECT YOUR FLEET. SWITCH BACK TO FIRE.', 'HITS AND MISSES BOTH END YOUR TURN. SINK ALL FIVE SHIPS TO WIN.'}
function B.preview(M,g,F,area)
 local cell=math.max(6,math.min(40,(area.w-48)//10,(area.h-79)//10));local side=cell*10;local x=area.x+(area.w-side)//2;local y=area.y+(area.h-side-55)//2+12
 F.bracket(x-12,y-12,side+24,side+24,24,9)
 local shots,down=0,0
 for i=1,100 do if g.sb:sub(i,i)=='1'then shots=shots+1;local xx,yy=x+(i-1)%10*cell,y+(i-1)//10*cell
  if g.b:sub(i,i)~='0'then smudge.rect(xx+1,yy+1,cell-2,cell-2,true,true)else local dot=math.max(3,cell//4);smudge.rounded_rect(xx+(cell-dot)//2,yy+(cell-dot)//2,dot,dot,dot//2,true,true,1)end
 end end
 for i=1,5 do if sunk(g.b,g.sb,i)then down=down+1 end end
 for n=1,9 do smudge.rect(x+n*cell,y,1,side,true,true);smudge.rect(x,y+n*cell,side,1,true,true)end;smudge.rect(x-1,y-1,side+2,side+2,false,1,true)
 local a,b=tostring(shots),tostring(down);local width=14+6+F.width(a,10)+24+14+6+F.width(b,10);local left=area.x+(area.w-width)//2;local top=y+side+24
 F.ring(left+7,top+7,7,false,2);F.text(a,left+20,top,10);left=left+20+F.width(a,10)+24;smudge.rect(left,top,14,14,true,true);F.text(b,left+20,top,10)
end

return B
