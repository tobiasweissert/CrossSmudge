-- GoScreens.cpp: fixed goban, seat bands and two-tap aim, shared by all sizes.
local B={}
function B.layout(w,h,n)
 local landscape=w>h;local side=landscape and math.min(300,h-156)or math.min(448,h-310,w-32)
 local pitch=side==448 and (n==9 and 49 or n==13 and 33 or 23)or math.max(8,(side-24)//(n-1))
 local pad=(side-pitch*(n-1))//2;local x=landscape and 24 or(w-side)//2;local y=landscape and 125 or 174
 local panel=landscape and x+side+44 or x;local panelW=landscape and w-panel-16 or side
 return {x=x,y=y,side=side,pitch=pitch,x0=x+pad,y0=y+pad,radius=math.max(3,pitch//2-2),panel=panel,panelW=panelW,landscape=landscape,pass={x=landscape and panel or 16,y=h-68,w=landscape and panelW or w-32,h=52}}
end
local cachedBoard,cachedDead,cachedKomi,cachedOwners,cachedBlack,cachedWhite
local function owners(M,g)
 local b=g.b;for p=1,#b do if M.at(g.dead,p)==1 then b=M.put(b,p,0)end end
 local seen,owner={},{}
 for p=1,#b do if M.at(b,p)>0 then owner[p]=M.at(b,p)elseif not seen[p]then
  local region={p};local mask=0;seen[p]=true;local i=1
  while i<=#region do for _,q in ipairs(M.neighbours(g.n,region[i]))do local s=M.at(b,q);if s==0 then if not seen[q]then seen[q]=true;region[#region+1]=q end else mask=mask|(1<<s)end end;i=i+1 end
  local s=mask==2 and 1 or mask==4 and 2 or 0;for _,q in ipairs(region)do owner[q]=s end
 end end;return owner
end
function B.draw(M,g,aim,F,w,h,seat)
 seat=seat or 1
 local l=B.layout(w,h,g.n);local a,b
 if g.phase>0 then if cachedBoard~=g.b or cachedDead~=g.dead or cachedKomi~=g.komi then cachedBoard,cachedDead,cachedKomi=g.b,g.dead,g.komi;cachedBlack,cachedWhite=M.score(g);cachedOwners=owners(M,g)end;a,b=cachedBlack,cachedWhite end
 local function half(v)return tostring(v//2)..'.'..(v%2*5)end
 local winner=a and(a>b and 1 or 2);F.header(g.phase==0 and'GO'or g.phase==1 and'COUNTING'or g.solo==1 and(winner==seat and'YOU WIN'or'THEY WIN')or winner==1 and'BLACK WINS'or'WHITE WINS',g.phase==0 and tostring(g.moves)or(a>b and'B+'or'W+')..half(math.abs(a-b)))
 local function seatBand(y,s,label)
  local left,width=l.panel,l.panelW;local active=g.turn==s;smudge.rect(left,y,width,58,active,3,true)
  local cx,cy=left+34,y+29;F.disc(cx,cy,18,not active);F.disc(cx,cy,15,(s==1)~=active)
  F.text(label,left+66,y+16,20,active);local tail=(s==1 and 'BLACK' or 'WHITE')..'  '..g.captured[s]..' TAKEN';F.text(tail,left+width-16-F.inkWidth(tail,10),y+22,10,active)
 end
 if g.phase==0 then seatBand(l.landscape and 120 or 95,3-seat,g.solo==1 and 'THEM' or 'THAT SIDE')end
 smudge.rect(l.x-9,l.y-9,l.side+18,l.side+18,false,9,true)
 for i=0,g.n-1 do local weight=(i==0 or i==g.n-1)and 3 or 1;smudge.rect(l.x0+i*l.pitch-weight//2,l.y0,weight,l.pitch*(g.n-1)+1,true,true);smudge.rect(l.x0,l.y0+i*l.pitch-weight//2,l.pitch*(g.n-1)+1,weight,true,true)end
 local near=g.n==9 and 2 or 3;local far=g.n-1-near;local mid=g.n//2;local stars={{near,near},{near,far},{far,near},{far,far},{mid,mid},{near,mid},{mid,near},{far,mid},{mid,far}}
 for i=1,g.n==19 and 9 or 5 do local p=stars[i];F.disc(l.x0+p[2]*l.pitch,l.y0+p[1]*l.pitch,g.n==9 and 5 or g.n==13 and 4 or 3,true)end
 local territory=g.phase>0 and cachedOwners
 for p=1,#g.b do local s=M.at(g.b,p);local x=l.x0+(p-1)%g.n*l.pitch;local y=l.y0+(p-1)//g.n*l.pitch;local dead=M.at(g.dead,p)==1
  if s>0 and not(g.phase==2 and dead)then
   if dead then F.ditherDisc(x,y,l.radius,false);if s==1 then F.ditherDisc(x,y,l.radius-3,true)else F.disc(x,y,l.radius-3,false)end else F.ring(x,y,l.radius,s==1,3)end
   if g.phase==0 and p==g.last then local outer=math.max(4,l.radius*8//22);F.disc(x,y,outer,s==2);F.disc(x,y,math.min(outer-2,l.radius*5//22),s==1)end
  end
  if territory and(s==0 or dead)and territory[p]~=0 then local mark=math.max(2,l.pitch*9//49-(g.phase==1 and dead and 2 or 0));local white=territory[p]==2
   smudge.rect(x-mark,y-mark,mark*2,mark*2,not white,true);if white then smudge.rect(x-mark,y-mark,mark*2,mark*2,false,g.phase==1 and dead and 1 or 3,true)end
  end
 end
 if aim and g.phase==0 then local x=l.x0+(aim-1)%g.n*l.pitch;local y=l.y0+(aim-1)//g.n*l.pitch;F.disc(x,y,l.radius,true);F.ditherDisc(x,y,l.radius-3,seat==1);F.bracket(x-l.pitch//2,y-l.pitch//2,l.pitch,l.pitch,l.pitch//4,g.n==9 and 4 or 3)end
 local lower=l.landscape and 188 or l.y+l.side+21
 if g.phase==0 then seatBand(l.landscape and 198 or lower,seat,g.solo==1 and 'YOU' or 'THIS SIDE');F.status('PASS',l.pass)
  local note
  if aim then local _,_,_,libs=M.trial(g,aim);if libs then if M.eye(g,aim)then note='THAT FILLS YOUR OWN EYE'elseif libs==1 then note='THAT STONE WOULD BE IN ATARI'end end end
  if not note and g.moves>0 and g.last==0 and(g.solo==0 or g.turn==seat)then note='THEY PASSED'end
  if note then F.nativeText(note,l.landscape and l.panel or l.x,l.pass.y-30,l.landscape and l.panelW or l.side,26,10,nil,false,true)end
 else
  local band=l.landscape and 198 or l.y+l.side+12
  local black='BLACK  '..(a//2)..(g.phase==2 and g.handicap>0 and'  ('..g.handicap..' STONES)'or'')
  local white='WHITE  '..half(b)..(g.phase==2 and'  (KOMI '..half(g.komi)..')'or'')
  F.nativeText(black,l.panel,band,g.phase==1 and math.min(220,l.panelW)or l.panelW,34,20,'left',false,true)
  F.nativeText(white,l.panel,band+36,g.phase==1 and math.min(220,l.panelW)or l.panelW,34,20,'left',false,true)
  if not l.landscape then l.pass.x=32;l.pass.y=h-84;l.pass.w=w-216 else l.pass.w=l.panelW-152 end;l.other={x=l.pass.x+l.pass.w+12,y=l.pass.y,w=140,h=52};F.status(g.phase==2 and'PLAY AGAIN'or'ACCEPT',l.pass);F.status(g.phase==2 and'DONE'or'PLAY ON',l.other)
  if g.phase==1 then F.nativeText('TAP A DEAD GROUP',l.landscape and l.panel or l.x+220,l.landscape and band+80 or band,l.landscape and l.panelW or l.side-220,70,10,'right',false,true)end
 end
 return l
end
function B.hit(l,n,x,y)
 if x<l.x or y<l.y or x>=l.x+l.side or y>=l.y+l.side then return end
 local col=(x-l.x0+l.pitch//2)//l.pitch;local row=(y-l.y0+l.pitch//2)//l.pitch
 if col>=0 and row>=0 and col<n and row<n then return row*n+col+1 end
end
function B.preview(M,g,F,area,inProgress,P)
 local pitch=240//(g.n-1);local span=pitch*(g.n-1);local radius=pitch//2
 local block=span+60;local top=area.y+(area.h>block and(area.h-block)//2 or 12);local x=area.x+(area.w-span)//2;local y=top+12
 if area.h<block then -- The original fixed mini-board overflows landscape; fit it above its doors.
  pitch=math.max(3,(area.h-54)//(g.n-1));span=pitch*(g.n-1);radius=math.max(2,pitch//2);x=area.x+(area.w-span)//2;y=area.y+10
 end
 for i=0,g.n-1 do smudge.line(x+i*pitch,y,x+i*pitch,y+span,true);smudge.line(x,y+i*pitch,x+span,y+i*pitch,true)end
 for p=1,#g.b do local s=M.at(g.b,p);if s>0 then local cx=x+(p-1)%g.n*pitch;local cy=y+(p-1)//g.n*pitch;F.disc(cx,cy,radius,true);F.disc(cx,cy,radius-2,s==1)end end
 local caption=inProgress and('IN PROGRESS   '..g.n..'x'..g.n..'   MOVE '..g.moves)or('LAST GAME: '..(P.lastWon==1 and'WON'or'LOST')..' BY '..(P.margin//2)..'.'..(P.margin%2*5))
 F.nativeText(caption,area.x,y+span+20,area.w,24,10)
end
return B
