-- Original ToyBattleScreens slab board, normalized terrain anchors and card marks.
local maps=dofile('geometry.lua');local B={}
function B.layout(w,h,map)
 local landscape=w>h;local width=landscape and 448 or math.min(448,w-32);local top=121;local rackY=h-158;local height=rackY-16-top
 if landscape then width=math.min(448,w-260);rackY=h-100;height=h-222 end
 local l={x=landscape and 16 or (w-width)//2,y=top,w=width,h=height,points={},rack={},action={x=16,y=h-68,w=landscape and width-156 or w-204,h=52},brief={x=landscape and width-36 or w-68,y=h-68,w=52,h=52}}
 local inset=30;for i,p in ipairs(maps[map].points)do l.points[i]={l.x+inset+p[1]*(width-inset*2)//1000,top+inset+p[2]*(height-inset*2)//1000}end
 local rx=landscape and w-220 or (w-432)//2
 for i=1,8 do l.rack[i]={x=rx+((i-1)%(landscape and 4 or 8))*54+3,y=(landscape and 154+(i-1)//4*78 or rackY)+3,w=48,h=64}end
 return l
end
local function glyph(x,y,size,what,paper)
 local half=size//2;local ink=not paper
 local function tri(dir)
  for i=0,size do local span=dir==3 and math.max(0,half-math.abs(i-half))or(dir==1 and i//2 or(size-i)//2)
   if dir==3 then smudge.rect(x-half+i,y-span,1,2*span+1,true,ink)else smudge.rect(x-span,y-half+i,2*span+1,1,true,ink)end
  end
 end
 if what==1 or what==4 then tri(1);if what==4 then smudge.rect(x-half,y+half,size,2,true,ink)end
 elseif what==2 then tri(2)elseif what==3 then tri(3)
 elseif what==5 then smudge.rect(x-half,y-half+1,size,size-2,true,ink)
 elseif what==6 then smudge.rect(x-half,y-half,size,2,true,ink);smudge.rect(x-half,y+half-2,size,2,true,ink)
 elseif what==7 then for i=-1,1 do smudge.line(x-half+i,y-half,x+half+i,y+half,ink);smudge.line(x+half+i,y-half,x-half+i,y+half,ink)end end
end
local function troop(x,y,k,F)
 if k==2 then glyph(x-5,y,12,2);glyph(x+5,y,12,2)
 elseif k==3 then smudge.rect(x-4,y+3,12,2,true,true);smudge.rect(x+6,y-6,2,11,true,true);for n=0,6 do smudge.rect(x-8+n,y+4-n//2,1,1+n,true,true)end
 elseif k==4 then glyph(x,y,14,7)elseif k==5 then glyph(x,y,14,3)
 elseif k==6 then F.ring(x,y,7,false,2);smudge.circle(x,y,2,true,true)
 elseif k==7 then glyph(x,y,14,2)end
end
B.glyph=glyph;B.troopMark=troop
function B.draw(M,g,chosen,slot,F,w,h)
 local t=M.board(g);local l=B.layout(w,h,g.map);local m=maps[g.map]
 F.header('TOY BATTLE',g.medals[1]..':'..g.medals[2]..' / '..t.goal)
 F.center(g.phase==1 and(g.winner==1 and 'YOU WIN' or 'THEY WIN')or g.turn==2 and 'THEIR TURN' or chosen and 'TAP A BASE' or 'CHOOSE A TROOP OR DRAW TWO',16,95,l.w,22,10)
 local function point(p)return l.x+30+p[1]*(l.w-60)//1000,l.y+30+p[2]*(l.h-60)//1000 end
 for p=1,#l.points do for q=p+1,#l.points do if(t.adj[p]&(1<<(q-1)))~=0 then local a,b=l.points[p],l.points[q];smudge.line(a[1],a[2],b[1],b[2],true,5) end end end
 for i,p in ipairs(m.medals)do if(g.regions&(1<<(i-1)))==0 then local x,y=point(p);local count=t.regions[i][2];local r=w>h and 5 or 7;local pitch=2*r+3;local span=count*pitch-3;smudge.rounded_rect(x-span//2-5,y-r-4,span+10,2*r+8,8,true,false,1);for n=0,count-1 do smudge.circle(x-span//2+r+n*pitch,y,r,true,true)end end end
 local side=w>h and 36 or 52;local half=side//2;local cut=side==52 and 20 or 10
 local reach=chosen and M.reach(g,1)
 for i,p in ipairs(l.points)do local x,y=p[1]-half,p[2]-half;local seat,k=M.top(g,i);local special=i<=t.n and g.special==1 and t.special[i]or 0;local gated=g.special==1 and t.gate[i]~=0;local radius=(gated or special==6)and 0 or 8
  smudge.rect_dither(x,y,side,side,seat>0);smudge.rounded_rect(x,y,side,side,radius,false,true,i>t.n and 5 or special>0 and 6 or 3)
  if i>t.n then F.center(t.hq[i-t.n]==1 and 'H' or 'E',x,y,side,side,cut)
  elseif seat>0 then if seat==1 then smudge.rounded_rect(x+5,y+5,side-10,side-10,6,true,true,1)end;F.center(k==1 and '*' or tostring(k-1),x,y,side,side,cut,seat==1)
   local depth=0;for _,p in ipairs(g.slots)do if p==i then depth=depth+1 end end;for n=1,math.min(depth-1,3)do smudge.rect(x+side-3,y+4+(n-1)*7,3,5,true,true)end
  end
  if special>0 then smudge.circle(x+side-2,y+2,w>h and 6 or 9,true,true);glyph(x+side-2,y+2,w>h and 6 or 8,special,true)end
  if gated then local label='';for k=1,8 do if(t.gate[i]&(1<<(k-1)))~=0 then label=label..(k==1 and '*'or tostring(k-1))end end;local yy=y+side-2+20<=l.y+l.h and y+side-2 or y-18;smudge.rounded_rect(x+3,yy,side-6,20,5,true,false,1);smudge.rounded_rect(x+3,yy,side-6,20,5,false,true,2);F.center(label,x+3,yy,side-6,20,10)end
  if chosen and g.phase==0 and g.turn==1 and M.canplace(g,1,i,chosen,true,reach)then F.bracket(x-5,y-5,side+10,side+10,12,4)end
  if slot==i then F.bracket(x-8,y-8,side+16,side+16,14,4)end
 end
 l.kinds={};for k=1,8 do for n=1,g.rack[1][k]do l.kinds[#l.kinds+1]=k end end
 for i,b in ipairs(l.rack)do local k=l.kinds[i]
  if k then local live=false;for p=1,#l.points do if M.canplace(g,1,p,k,true)then live=true;break end end
   if live or g.frozen[1]==k then smudge.rect_dither(b.x,b.y,b.w,b.h,g.frozen[1]==k)end
   smudge.rounded_rect(b.x,b.y,b.w,b.h,8,false,true,chosen==k and 5 or 2);F.center(k==1 and '*'or tostring(k-1),b.x,b.y+3,b.w,35,20);troop(b.x+b.w//2,b.y+49,k,F)
  else smudge.rounded_rect(b.x,b.y,b.w,b.h,8,false,true,2)end
 end
 F.pill(g.phase==1 and 'HOW IT ENDED' or g.turn==2 and 'WAIT' or chosen and 'CANCEL' or 'DRAW 2',l.action,true);F.pill('?',l.brief,false)
 local cx=l.action.x+l.action.w+16;for row=0,1 do local s=row==0 and 2 or 1;local yy=h-58+row*16;local hand,discard=0,0;for k=1,8 do hand=hand+g.rack[s][k];discard=discard+g.discard[s][k]end
  smudge.rect(cx,yy+3,7,11,true,true);F.text(tostring(hand),cx+10,yy,10);glyph(cx+28,yy+8,9,2);F.text(tostring(20-g.taken[s]),cx+36,yy,10);glyph(cx+62,yy+8,9,7);F.text(tostring(discard),cx+70,yy,10)
 end
 return l
end
function B.hit(l,x,y)
 for i,p in ipairs(l.points)do if(x-p[1])^2+(y-p[2])^2<=32^2 then return 'slot',i end end
 for i,b in ipairs(l.rack)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then return 'troop',l.kinds[i]end end
end
function B.preview(M,g,F,area)
 local t=M.board(g);local m=maps[g.map];local node=math.max(5,math.min(18,math.min(area.w,area.h-32)//8));local pad=node//2+6;local points={};local height=area.h-34
 for i,p in ipairs(m.points)do points[i]={area.x+pad+p[1]*(area.w-2*pad)//1000,area.y+pad+p[2]*(height-2*pad)//1000}end
 for p=1,#points do for q=p+1,#points do if(t.adj[p]&(1<<(q-1)))~=0 then smudge.line(points[p][1],points[p][2],points[q][1],points[q][2],true,2)end end end
 for i,p in ipairs(points)do local x,y=p[1]-node//2,p[2]-node//2;local seat=M.top(g,i);local radius=node>=14 and t.gate[i]~=0 and 0 or 4
  if seat==1 then smudge.rect_dither(x,y,node,node,true)else smudge.rounded_rect(x,y,node,node,radius,true,seat==0,1)end
  smudge.rounded_rect(x,y,node,node,radius,false,true,2)
  if i>t.n then smudge.rounded_rect(x-3,y-3,node+6,node+6,3,false,true,2)end
 end
 F.center(F.fit(t.name..'   '..t.goal..' TO WIN',area.w,10),area.x,area.y+height+10,area.w,20,10)
end
return B
