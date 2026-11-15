-- GoScreens.cpp: fixed goban, seat bands and two-tap aim, shared by all sizes.
local B={}
function B.layout(w,h,n)
 local landscape=w>h;local side=landscape and math.min(300,h-156)or math.min(448,h-310,w-32)
 local pitch=side==448 and (n==9 and 49 or n==13 and 33 or 23)or math.max(8,(side-24)//(n-1))
 local pad=(side-pitch*(n-1))//2;local x=landscape and 24 or(w-side)//2;local y=landscape and 125 or 178
 local panel=landscape and x+side+44 or x;local panelW=landscape and w-panel-16 or side
 return {x=x,y=y,side=side,pitch=pitch,x0=x+pad,y0=y+pad,radius=math.max(3,pitch//2-2),panel=panel,panelW=panelW,landscape=landscape,pass={x=landscape and panel or 16,y=h-68,w=landscape and panelW or w-32,h=52}}
end
local function owners(M,g)
 local b=g.b;for p=1,#b do if M.at(g.dead,p)==1 then b=M.put(b,p,0)end end
 local seen,owner={},{}
 for p=1,#b do if M.at(b,p)>0 then owner[p]=M.at(b,p)elseif not seen[p]then
  local region={p};local mask=0;seen[p]=true;local i=1
  while i<=#region do for _,q in ipairs(M.neighbours(g.n,region[i]))do local s=M.at(b,q);if s==0 then if not seen[q]then seen[q]=true;region[#region+1]=q end else mask=mask|(1<<s)end end;i=i+1 end
  local s=mask==2 and 1 or mask==4 and 2 or 0;for _,q in ipairs(region)do owner[q]=s end
 end end;return owner
end
function B.draw(M,g,aim,F,w,h)
 local l=B.layout(w,h,g.n);local a,b;if g.phase>0 then a,b=M.score(g)end
 F.header(g.phase==0 and 'GO' or g.phase==1 and 'COUNTING' or a>b and 'BLACK WINS' or 'WHITE WINS',g.phase==0 and tostring(g.moves)or (a>b and 'B+' or 'W+')..math.abs(a-b)/2)
 local function seat(y,s,label)
  local left,width=l.panel,l.panelW;local active=g.turn==s;smudge.rect(left,y,width,58,active,3,true)
  local cx,cy=left+34,y+29;smudge.circle(cx,cy,18,true,not active);smudge.circle(cx,cy,15,true,(s==1)~=active)
  F.text(label,left+66,y+16,20,active);local tail=(s==1 and 'BLACK' or 'WHITE')..'  '..g.captured[s]..' TAKEN';F.text(tail,left+width-16-F.width(tail,10),y+22,10,active)
 end
 if g.phase==0 then seat(l.landscape and 120 or 95,2,g.solo==1 and 'THEM' or 'THAT SIDE')end
 smudge.rect(l.x-9,l.y-9,l.side+18,l.side+18,false,9,true)
 for i=0,g.n-1 do local weight=(i==0 or i==g.n-1)and 3 or 1;smudge.rect(l.x0+i*l.pitch-weight//2,l.y0,weight,l.pitch*(g.n-1)+1,true,true);smudge.rect(l.x0,l.y0+i*l.pitch-weight//2,l.pitch*(g.n-1)+1,weight,true,true)end
 local near=g.n==9 and 2 or 3;local far=g.n-1-near;local mid=g.n//2;local stars={{near,near},{near,far},{far,near},{far,far},{mid,mid},{near,mid},{mid,near},{far,mid},{mid,far}}
 for i=1,g.n==19 and 9 or 5 do local p=stars[i];smudge.circle(l.x0+p[2]*l.pitch,l.y0+p[1]*l.pitch,g.n==9 and 5 or g.n==13 and 4 or 3,true,true)end
 local territory=g.phase>0 and owners(M,g)
 for p=1,#g.b do local s=M.at(g.b,p);local x=l.x0+(p-1)%g.n*l.pitch;local y=l.y0+(p-1)//g.n*l.pitch;local dead=M.at(g.dead,p)==1
  if s>0 and not(g.phase==2 and dead)then
   if dead then F.ditherDisc(x,y,l.radius,false);F.ditherDisc(x,y,l.radius-3,s==1)else F.ring(x,y,l.radius,s==1,3)end
   if g.phase==0 and p==g.last then local r=math.max(4,l.radius*8//22);smudge.circle(x,y,r,true,s==2);smudge.circle(x,y,math.max(1,r-2),true,s==1)end
  end
  if territory and (s==0 or dead)and territory[p]~=0 then local mark=math.max(2,l.pitch*9//49-(dead and 2 or 0));smudge.rect(x-mark,y-mark,mark*2,mark*2,true,territory[p]==1);smudge.rect(x-mark,y-mark,mark*2,mark*2,false,2,true)end
 end
 if aim and g.phase==0 then local x=l.x0+(aim-1)%g.n*l.pitch;local y=l.y0+(aim-1)//g.n*l.pitch;F.ditherDisc(x,y,l.radius,g.turn==1);F.bracket(x-l.pitch//2,y-l.pitch//2,l.pitch,l.pitch,l.pitch//4,3)end
 local lower=l.landscape and 188 or l.y+l.side+25
 if g.phase==0 then seat(l.landscape and 198 or lower,1,g.solo==1 and 'YOU' or 'THIS SIDE');F.pill(g.solo==1 and g.turn==2 and 'THEIR TURN' or 'PASS',l.pass,true)
 else
  local function half(v)return tostring(v//2)..(v%2==1 and '.5' or '')end
  F.text('BLACK  '..half(a),l.panel,lower,20);F.text('WHITE  '..half(b)..(g.phase==2 and '  (KOMI '..half(g.komi)..')' or ''),l.panel,lower+36,20)
  l.pass.w=l.panelW-152;l.other={x=l.pass.x+l.pass.w+12,y=h-68,w=140,h=52};F.pill(g.phase==2 and 'PLAY AGAIN' or (g.accepted&2)~=0 and 'WHITE ACCEPTS' or 'BLACK ACCEPTS',l.pass,true);F.pill(g.phase==2 and 'DONE' or 'PLAY ON',l.other,false)
  if g.phase==1 then F.center('TAP A DEAD GROUP',l.landscape and l.panel or l.x+l.side//2,l.landscape and lower+80 or lower,l.landscape and l.panelW or l.side//2,70,10)end
 end
 return l
end
function B.hit(l,n,x,y)
 if x<l.x or y<l.y or x>=l.x+l.side or y>=l.y+l.side then return end
 local col=(x-l.x0+l.pitch//2)//l.pitch;local row=(y-l.y0+l.pitch//2)//l.pitch
 if col>=0 and row>=0 and col<n and row<n then return row*n+col+1 end
end
function B.preview(M,g,F,area)
 local pitch=math.max(4,math.min(240,area.h-52,area.w-30)//(g.n-1));local span=pitch*(g.n-1);local x=area.x+(area.w-span)//2;local y=area.y+(area.h-span-34)//2
 for i=0,g.n-1 do smudge.line(x+i*pitch,y,x+i*pitch,y+span,true);smudge.line(x,y+i*pitch,x+span,y+i*pitch,true)end
 for p=1,#g.b do local s=M.at(g.b,p);if s>0 then F.ring(x+(p-1)%g.n*pitch,y+(p-1)//g.n*pitch,math.max(2,pitch//2-2),s==1,2)end end
 F.center((g.phase==0 and 'IN PROGRESS' or 'COUNTED GAME')..'   '..g.n..'X'..g.n..'   MOVE '..g.moves,area.x,y+span+20,area.w,22,10)
end
return B
