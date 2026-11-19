-- Original HexScreens.cpp axial hexagons, stones and corner seat cards.
local B={}
function B.record(index)B.last=index end
function B.layout(w,h)
 local available=w-32;local room=h-111;local a=math.max(2,available//34);local hh=math.floor((a*1732+500)/1000)
 while a>2 and (34*a+2*(hh//4+3)>available or 32*hh+2*(hh//4+3)>room)do a=a-1;hh=math.floor((a*1732+500)/1000)end
 return {a=a,half=hh,x=(w-34*a)//2,y=95+hh//4+3,w=w,h=h}
end
local function center(l,col,row)return l.x+2*l.a+3*l.a*col,l.y+l.half+2*l.half*row+l.half*col end
local function vertices(l,x,y)
 return {{x+2*l.a,y},{x+l.a,y+l.half},{x-l.a,y+l.half},{x-2*l.a,y},{x-l.a,y-l.half},{x+l.a,y-l.half}}
end
local function polygon(points,paper)
 local lo,hi=points[1][2],points[1][2];for _,p in ipairs(points)do lo=math.min(lo,p[2]);hi=math.max(hi,p[2])end
 for y=lo,hi do local left,right=10000,-10000
  for n=1,#points do local p,q=points[n],points[n%#points+1]
   if p[2]~=q[2] and y>=math.min(p[2],q[2])and y<=math.max(p[2],q[2])then local xx=p[1]+(y-p[2])*(q[1]-p[1])/(q[2]-p[2]);left=math.min(left,xx);right=math.max(right,xx)end
  end
  if right>=left then smudge.rect(math.ceil(left),y,math.floor(right)-math.ceil(left)+1,1,true,not paper)end
 end
end
function B.chain(g)
 if g.status==0 then return {}end
 local queue,previous,head={},{},1;local seat=g.status
 for n=0,10 do local at=seat==1 and n+1 or n*11+1
  if g.b[at]==seat then queue[#queue+1]=at;previous[at]=0 end
 end
 local offsets={{1,0},{0,1},{-1,1},{-1,0},{0,-1},{1,-1}}
 while head<=#queue do local at=queue[head];head=head+1;local col,row=(at-1)%11,(at-1)//11
  if (seat==1 and row==10)or(seat==2 and col==10)then
   local path={};while at~=0 do path[#path+1]=at;at=previous[at]end;return path
  end
  for _,offset in ipairs(offsets)do local c,r=col+offset[1],row+offset[2]
   if c>=0 and c<11 and r>=0 and r<11 then local next=r*11+c+1
    if previous[next]==nil and g.b[next]==seat then previous[next]=at;queue[#queue+1]=next end
   end
  end
 end
 return {}
end
local function paintBoard(g,F,l,radius)
 local depth=l.half//4+2
 local strips={};local neighbors={{1,0},{0,1},{-1,1},{-1,0},{0,-1},{1,-1}}
 for row=0,10 do for col=0,10 do
  local x,y=center(l,col,row);local points=vertices(l,x,y);polygon(points,true)
  for n=1,6 do local p,q=points[n],points[n%6+1];smudge.line(p[1],p[2],q[1],q[2],true)end
  local v=g.b[row*11+col+1]
  if v~=0 then F.ring(x,y,radius,v==1,3)end
  for n,offset in ipairs(neighbors)do local nc,nr=col+offset[1],row+offset[2]
   if nc<0 or nc>10 or nr<0 or nr>10 then
    local p,q=points[n],points[n%6+1];local dx,dy=q[1]-p[1],q[2]-p[2];local length=math.sqrt(dx*dx+dy*dy)
    local nx,ny=math.floor(depth*dy/length+.5),math.floor(-depth*dx/length+.5)
    strips[#strips+1]={p=p,q=q,op={p[1]+nx,p[2]+ny},oq={q[1]+nx,q[2]+ny},owner=(nr<0 or nr>10)and 1 or 2}
   end
  end
 end end
 -- Join offset edges at their intersections: a continuous mitred band.
 for _,a in ipairs(strips)do for _,b in ipairs(strips)do
  if a.q[1]==b.p[1] and a.q[2]==b.p[2] then
   local ax,ay=a.q[1]-a.p[1],a.q[2]-a.p[2];local bx,by=b.q[1]-b.p[1],b.q[2]-b.p[2];local cross=ax*by-ay*bx
   if cross~=0 then local t=((b.op[1]-a.op[1])*by-(b.op[2]-a.op[2])*bx)/cross
    local point={math.floor(a.op[1]+t*ax+.5),math.floor(a.op[2]+t*ay+.5)};a.oq=point;b.op=point
   end
  end
 end end
 for owner=2,1,-1 do for _,edge in ipairs(strips)do if edge.owner==owner then
  polygon({edge.p,edge.q,edge.oq,edge.op},owner==2)
  if owner==2 then smudge.line(edge.p[1],edge.p[2],edge.q[1],edge.q[2],true);smudge.line(edge.op[1],edge.op[2],edge.oq[1],edge.oq[2],true)end
 end end end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h);local count=0;for _,v in ipairs(g.b)do if v~=0 then count=count+1 end end
 F.header(g.status>0 and (mode==2 and (g.status==1 and 'BLACK WINS' or 'WHITE WINS')or g.status==1 and 'YOU WIN' or 'THEY WIN')or 'HEX',search and 'THINKING' or tostring(count))
 local radius=math.max(3,math.min(l.half-3,2*l.a-4));paintBoard(g,F,l,radius)
 if B.last and g.status==0 and g.b[B.last]~=0 then
  local x,y=center(l,(B.last-1)%11,(B.last-1)//11);local black=g.b[B.last]==1
  smudge.circle(x,y,math.max(3,radius*8//22+2),true,not black);smudge.circle(x,y,math.max(1,radius*5//22+1),true,black)
 end
 for _,at in ipairs(B.chain(g))do local x,y=center(l,(at-1)%11,(at-1)//11);local black=g.b[at]==1
  smudge.circle(x,y,math.max(3,radius*8//22+2),true,not black);smudge.circle(x,y,math.max(1,radius*5//22+1),true,black)
 end
 local function card(x,y,side,label,active)
  local width,height=l.a*17,l.half*4;local r=math.min(17,height//4)
  smudge.rect(x,y,width,height,active,active and true or 3,true)
  local edges=side==1 and 'TOP TO BOTTOM' or 'LEFT TO RIGHT';local size=height<70 and 10 or 20
  local room=width-16-r*2-12;local text=math.min(room,math.max(F.width(label,size),F.width(edges,10)))
  local left=x+(width-(r*2+12+text))//2;local cx,cy=left+r,y+height//2
  smudge.circle(cx,cy,r,true,not active);smudge.circle(cx,cy,r-3,true,(side==1 and not active)or(side~=1 and active))
  local tx=left+r*2+12;local cap=size==20 and 25 or 13;local yy=y+(height-cap-8-13)//2
  F.text(F.fit(label,room,size),tx,yy,size,active);F.text(F.fit(edges,room,10),tx,yy+cap+8,10,active)
 end
 if g.status>0 then
  l.again={x=l.x+l.a*17,y=l.y,w=l.a*17,h=math.min(52,l.half*4)}
  l.done={x=l.x,y=l.y+l.half*32-math.min(52,l.half*4),w=l.a*17,h=math.min(52,l.half*4)}
  F.pill(w>h and 'AGAIN' or 'PLAY AGAIN',l.again,true);F.pill('DONE',l.done,true)
 else
  card(l.x+l.a*17,l.y,2,mode==2 and 'WHITE' or 'THEM',g.turn==2)
  card(l.x,l.y+l.half*28,1,mode==2 and 'BLACK' or 'YOU',g.turn==1)
 end
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 for row=0,10 do for col=0,10 do local cx,cy=center(l,col,row);local dx,dy=math.abs(x-cx),math.abs(y-cy)
  if dx<=2*l.a and dy<=l.half and (dx<=l.a or dy*l.a<=(2*l.a-dx)*l.half)then return 'cell',row*11+col+1 end
 end end
end
B.help={'CONNECT YOUR TWO OPPOSITE EDGES WITH AN UNBROKEN CHAIN.', 'BLACK CONNECTS TOP TO BOTTOM. WHITE CONNECTS LEFT TO RIGHT.', 'TAP AN EMPTY HEXAGON TO PLACE A STONE. STONES CANNOT BE MOVED.', 'EACH HEXAGON TOUCHES SIX NEIGHBORS. THERE ARE NO DIAGONAL GAPS.', 'THE LAST MOVE IS MARKED. A WINNING CHAIN IS MARKED WHEN THE GAME ENDS.', 'THE GAME CANNOT DRAW. THERE IS NO SWAP RULE.'}
function B.preview(M,g,F,area)
 local a=8;local hh=14;while a>2 and(34*a+12>area.w or 32*hh+12>area.h)do a=a-1;hh=math.floor((a*1732+500)/1000)end
 local l={a=a,half=hh,x=area.x+(area.w-34*a)//2,y=area.y+(area.h-32*hh)//2}
 paintBoard(g,F,l,math.max(2,hh-3))
end
return B
