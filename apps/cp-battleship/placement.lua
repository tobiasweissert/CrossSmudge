-- Native BattleshipActivity setup interaction, over the existing fleet strings.
-- The staged fleet is small transient UI state; core journals retain their
-- original format and partial placements until the explicit Ready action.
local P={};local lengths={5,4,3,3,2};local names={'CARRIER','BATTLESHIP','CRUISER','SUBMARINE','DESTROYER'}
local current,owner
local function hull(board,ship)
 local first=board:find(tostring(ship),1,true);if not first then return end
 local second=board:find(tostring(ship),first+1,true)
 return first,second==first+1
end
local function place(board,ship,bow,horizontal)
 local n=lengths[ship];local x,y=(bow-1)%10,(bow-1)//10
 if bow<1 or bow>100 or horizontal and x+n>10 or not horizontal and y+n>10 then return end
 local cleared=board:gsub(tostring(ship),'0')
 for k=0,n-1 do local i=bow+k*(horizontal and 1 or 10);if cleared:sub(i,i)~='0'then return end end
 for k=0,n-1 do local i=bow+k*(horizontal and 1 or 10);cleared=cleared:sub(1,i-1)..ship..cleared:sub(i+1)end
 return cleared
end
function P.ensure(M,g)
 if owner==g then return current end
 owner=g;local random=M.new(g.rng);current={board=g.a,rng=random.rng,selected=0,message='TAP A SHIP TO MOVE IT'}
 for ship=1,5 do if not hull(current.board,ship)then
  local bow,horizontal=hull(random.b,ship);local result=place(current.board,ship,bow,horizontal)
  if not result then for n=0,199 do result=place(current.board,ship,n%100+1,n<100);if result then break end end end
  if not result then smudge.log('Battleship staged fleet cannot be completed');return nil end
  current.board=result
 end end
 return current
end
function P.shuffle(M,g)
 local p=P.ensure(M,g);if not p then return false end
 local random=M.new(p.rng);p.board=random.b;p.rng=random.rng;p.selected=0;p.message='TAP A SHIP TO MOVE IT';return true
end
function P.select(p,ship)p.selected=ship;p.message='TAP A CELL, OR THE SHIP TO TURN'end
function P.tap(p,at)
 if at<1 or at>100 then return false end
 local ship=tonumber(p.board:sub(at,at))
 if ship>0 and ship~=p.selected then P.select(p,ship);return true end
 if p.selected==0 then p.message='TAP A SHIP TO MOVE IT';return true end
 local bow,horizontal=hull(p.board,p.selected);local result
 if ship==p.selected then
  horizontal=not horizontal;result=place(p.board,p.selected,bow,horizontal)
  if not result then for back=1,lengths[p.selected]-1 do
   local row=(bow-1)//10-(horizontal and 0 or back);local col=(bow-1)%10-(horizontal and back or 0)
   if row<0 or col<0 then break end
   result=place(p.board,p.selected,row*10+col+1,horizontal);if result then break end
  end end
  if not result then p.message='IT WILL NOT TURN THERE';return true end
 else
  result=place(p.board,p.selected,at,horizontal)
  if not result then p.message='IT WILL NOT FIT THERE';return true end
  p.message='TAP A CELL, OR IT AGAIN TO TURN'
 end
 p.board=result;return true
end
function P.ready(M,g)
 local p=P.ensure(M,g);if not p then return false end
 -- Validate a complete candidate with the actual core before mutating the save.
 local candidate={};for key,value in pairs(g)do candidate[key]=value end
 candidate.a=p.board;candidate.ship=6;candidate.phase=1;candidate.show=0
 if not M.valid(candidate)then smudge.log('Battleship refused staged fleet');p.message='FLEET IS NOT READY';return false end
 g.a=p.board;g.ship=6;g.phase=1;g.show=0;return true
end
function P.draw(M,g,l,F,cursor,focus)
 local p=P.ensure(M,g);if not p then return nil end
 F.header('YOUR FLEET');F.text(F.fit(p.message,l.w-32,10),16,l.reportY,10)
 local side=l.cell*10
 for n=1,9 do smudge.rect(l.x+n*l.cell,l.y,1,side,true,true);smudge.rect(l.x,l.y+n*l.cell,side,1,true,true)end
 smudge.rect(l.x-4,l.y-4,side+8,side+8,false,4,true);F.bracket(l.x-9,l.y-9,side+18,side+18,24,4)
 for ship=1,5 do
  local bow,horizontal=hull(p.board,ship);local x=l.x+(bow-1)%10*l.cell;local y=l.y+(bow-1)//10*l.cell
  local width=horizontal and l.cell*lengths[ship]or l.cell;local height=horizontal and l.cell or l.cell*lengths[ship]
  smudge.rect_dither(x+2,y+2,width-4,height-4,false);smudge.rect(x+2,y+2,width-4,height-4,false,p.selected==ship and 4 or 1,true)
  if p.selected==ship then F.bracket(x,y,width,height,math.max(10,l.cell//3),4)end
 end
 local top=l.y+side+21;local left,width=16,l.w-32;local row=(l.view.y-top)//5
 if l.w>l.h then left=l.x+side+32;top=151;width=l.w-left-16;row=math.max(12,(l.view.y-top)//5)end
 l.roster={x=left,y=top,w=width,h=row*5,row=row}
 if row>=12 then for ship,name in ipairs(names)do
  local y=top+(ship-1)*row;local chosen=p.selected==ship
  if chosen then smudge.rect_dither(left,y,width,row-2,false);smudge.rect(left,y,width,row-2,false,3,true)end
  F.text(name,left+6,y+(row-2-13)//2,10)
  local pip=math.min(14,row-8)
  for n=1,lengths[ship]do local x=left+width-(pip+3)*(lengths[ship]-n+1);local yy=y+(row-2-pip)//2
   smudge.rect_dither(x,yy,pip,pip,false);smudge.rect(x,yy,pip,pip,false,chosen and 3 or 1,true)
  end
 end end
 F.status('SHUFFLE',l.rotate);F.status('READY',l.view)
 if focus then
  if cursor>100 then local b=cursor==101 and l.rotate or l.view;smudge.rect(b.x+4,b.y+4,b.w-8,b.h-8,false,2,false)
  else local x=l.x+(cursor-1)%10*l.cell;local y=l.y+(cursor-1)//10*l.cell;F.bracket(x+3,y+3,l.cell-6,l.cell-6,math.max(4,l.cell//5),2)end
 end
 l.placement=p;l.game=g;l.model=M;l.phase=0;return l
end
function P.hit(l,x,y,F)
 if F.inside(l.rotate,x,y)then P.shuffle(l.model,l.game);return 'refresh'end
 if F.inside(l.view,x,y)then P.ready(l.model,l.game);return 'refresh'end
 local r=l.roster
 if r and r.row>=12 and F.inside(r,x,y)then P.select(l.placement,(y-r.y)//r.row+1);return 'refresh'end
 if x>=l.x and x<l.x+l.cell*10 and y>=l.y and y<l.y+l.cell*10 then
  P.tap(l.placement,(y-l.y)//l.cell*10+(x-l.x)//l.cell+1);return 'refresh'
 end
end
return P
