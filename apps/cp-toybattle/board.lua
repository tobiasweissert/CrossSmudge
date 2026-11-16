local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_board.lua')end
-- Original ToyBattleScreens slab board, normalized terrain anchors and card marks.
local maps=dofile('geometry.lua');local B={};local art={}
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function glyph(x,y,size,what,paper)
 if what==0 then return end;local key='special-'..what..'-'..size
 art[key]=art[key]or assert(smudge.read_file('art/'..key..'.raw',129));smudge.bitmap(x-16,y-16,32,32,art[key],paper or false)
end
local function troop(x,y,k,F)
 if k==1 or k==8 then return end;if k==6 then F.disc(x,y,4,false)end;local key='mark-'..(k-1)..'-12';art[key]=art[key]or assert(smudge.read_file('art/'..key..'.raw',129));smudge.bitmap(x-16,y-16,32,32,art[key])
end
B.glyph=glyph;B.troopMark=troop
function B.layout(w,h,map)
 local l={x=(w-448)//2,y=121,w=448,h=509,points={},rack={},action=box(16,732,w-196,52),brief=box(w-68,732,52,52)}
 for i,p in ipairs(maps[map].points)do l.points[i]={l.x+30+p[1]*388//1000,151+p[2]*449//1000}end
 for i=1,8 do l.rack[i]=box((w-432)//2+(i-1)*54+3,645,48,64)end;return l
end
local function frame(F,b,r,weight)if r==0 then smudge.rect(b.x,b.y,b.w,b.h,false,weight,true)else smudge.rounded_rect(b.x,b.y,b.w,b.h,r,false,true,weight)end end
local function ground(F,b,r,dark)if r==0 then smudge.rect_dither(b.x,b.y,b.w,b.h,dark)else F.roundDither(b.x,b.y,b.w,b.h,r,dark)end end
local function centered(F,text,b,size,paper)F.native(text,b,size,'center',paper,1,true)end
local function gateLabel(mask)
 local lo,hi,count=-1,-1,0;for k=1,7 do if mask&(1<<k)~=0 then if lo<0 then lo=k end;hi=k;count=count+1 end end
 local out=mask&1~=0 and'*'or'';if count>1 and hi-lo+1==count then return out..lo..'-'..hi end
 for k=1,7 do if mask&(1<<k)~=0 then out=out..k end end;return out
end
local function face(F,b,k,live,frozen,chosen)
 F.box(b.x,b.y,b.w,b.h,true,true,1,8);if live or frozen then ground(F,b,8,frozen)end;frame(F,b,8,chosen and 5 or 2)
 if k==1 or k==8 then centered(F,k==1 and'*'or'7',b,20)else centered(F,tostring(k-1),box(b.x,b.y+b.h*9//64,b.w,b.h*26//64),20);troop(b.x+b.w//2,b.y+b.h-b.h*17//64,k,F)end
end
B.face=face
function B.draw(M,g,chosen,slot,F,w,h,draft)
 local t=M.board(g);local l=B.layout(w,h,g.map);local m=maps[g.map];local actual=draft and draft.original or g
 F.header('TOY BATTLE');local medals=g.medals[1]..'-'..g.medals[2]..' OF '..t.goal
 local medalWin=actual.phase==1 and actual.ending==2;local mw=F.nativeWidth(medals,20)
 if medalWin then F.box(w-16-mw-10,12,mw+20,52,true,true,1,6)end
 F.native(medals,box(0,17,w-16,42),20,'right',not medalWin)
 local prompt=draft and draft.prompt or g.phase==1 and(g.winner==1 and'YOU WIN'or'YOU LOSE')or g.turn==2 and'THEY ARE THINKING'or chosen and'TAP A BASE'or'TAP A TROOP, OR DRAW TWO'
 F.native(prompt,box(16,95,w-32,22),10,'center')
 for p=1,#l.points do for q=p+1,#l.points do if t.adj[p]&(1<<(q-1))~=0 then local a,b=l.points[p],l.points[q];F.nativeLine(a[1],a[2],b[1],b[2],5)end end end
 local function point(p)return l.x+30+p[1]*388//1000,151+p[2]*449//1000 end
 for i,p in ipairs(m.medals)do if g.regions&(1<<(i-1))==0 then local x,y=point(p);local count=t.regions[i][2];local span=count*17-3;F.box(x-span//2-5,y-11,span+10,22,true,true,1,8);for n=0,count-1 do F.disc(x-span//2+7+n*17,y,7,true)end end end
 for i,p in ipairs(l.points)do
  local b=box(p[1]-26,p[2]-26,52,52);local seat,k=M.top(g,i);local hq=i>t.n;if hq then seat=0 end;local special=g.special==1 and not hq and t.special[i]or 0;local gated=g.special==1 and t.gate[i]~=0;local r=(gated or special==6)and 0 or 8
  ground(F,b,r,seat~=0);frame(F,b,r,hq and 5 or special~=0 and 6 or 3)
  if gated then local y=b.y+b.h-2;local th=25;if y+th>630 then y=b.y-th+2 end;local tab=box(p[1]-23,y,46,th);F.box(tab.x,tab.y,46,th,true,true,1,5);frame(F,tab,5,2);F.native(gateLabel(t.gate[i]),box(tab.x,tab.y+2,tab.w,21),10,'center')end
  if hq then centered(F,t.hq[i-t.n]==1 and'H'or'E',b,20)
  else
   if seat~=0 then if seat==1 then F.box(b.x+6,b.y+6,40,40,true,false,1,6)end;centered(F,k==1 and'*'or tostring(k-1),b,20,seat==1);local depth=0;for _,at in ipairs(g.slots)do if at==i then depth=depth+1 end end;for n=1,math.min(3,depth-1)do smudge.rect(b.x+49,b.y+4+(n-1)*7,3,5,true,true)end end
   if special~=0 then F.disc(b.x+50,b.y+2,9,true);glyph(b.x+50,b.y+2,8,special,true)end
   local candidate;if draft then candidate=draft.candidates and draft.candidates[i]else candidate=chosen and M.canplace(g,1,i,chosen,true)end
   if candidate then F.bracket(b.x-5,b.y-5,62,62,12,4)end
  end
 end
 if actual.phase==1 and actual.ending==1 and #g.slots>0 then local p=l.points[g.slots[#g.slots]];F.bracket(p[1]-36,p[2]-36,72,72,14,5)end
 local rackGame=g;local rackSource=g.rack[1]
 if draft and draft.ask=='exhume'then rackSource={};for k=1,8 do rackSource[k]=draft.original.discard[1][k]end;for i=1,(draft.step or 1)-1 do local st=draft.steps[i];if st and st[5]==1 and t.special[st[1]]==4 then rackSource[st[8]]=math.max(0,rackSource[st[8]]-1)end end end
 l.kinds={};for k=1,8 do for n=1,rackSource[k]do l.kinds[#l.kinds+1]=k end end
 for i,b in ipairs(l.rack)do local k=l.kinds[i]
  if k then local live=draft and draft.ask=='exhume'or false;if not draft or draft.ask=='troop'then for pos=1,#l.points do if M.canplace(g,g.turn,pos,k,true)then live=true;break end end end;face(F,b,k,live,actual.frozen[1]==k,chosen==k)
  else frame(F,b,8,2)end
 end
 local ask=draft and draft.ask or(chosen and'slot'or'troop');local text=actual.phase==1 and'HOW IT ENDED'or actual.turn~=1 and'WAIT'or(chosen or draft and draft.step and draft.step>1)and'CANCEL'or'DRAW 2'
 local function button(text,b,enabled,rounded)
  if enabled then if rounded then F.box(b.x,b.y,b.w,b.h,true,false,1,20)else smudge.rect(b.x,b.y,b.w,b.h,true,true)end else smudge.rect_dither(b.x,b.y,b.w,b.h,true)end
  F.button(text,b,nil,false,true)
 end
 if actual.phase==0 and actual.turn==1 and ask~='troop'and ask~='slot'then
  l.skip=box(l.action.x,l.action.y,(l.action.w-12)//2,52);l.take=box(l.skip.x+l.skip.w+12,l.skip.y,l.skip.w,52);l.targeted=ask=='jumbo'or ask=='recall'or ask=='shoveFrom'or ask=='shoveTo'or ask=='exhume';button('SKIP',l.skip,true);button(l.targeted and'BACK'or'TAKE',l.take,true)
 else
  local live=actual.phase==1 or actual.turn==1;if text=='DRAW 2'then local count=0;for _,n in ipairs(rackGame.rack[1])do count=count+n end;live=live and count<8 and rackGame.taken[1]<20 end;button(text,l.action,live,actual.phase==1)
 end;button('?',l.brief,true)
 local cx=l.action.x+l.action.w+12;local countsTop=742
 for row=0,1 do local seat=row==0 and 2 or 1;local yy=countsTop+row*16;local hand,discard=0,0;for k=1,8 do hand=hand+g.rack[seat][k];discard=discard+g.discard[seat][k]end
  local function count(x,value,mark)if mark==0 then smudge.rect(x,yy+3,7,11,true,true)else glyph(x+4,yy+8,9,mark)end;F.nativeLabel(tostring(value),box(x+10,yy,20,16),10,'left')end
  count(cx,hand,0);count(cx+24,20-g.taken[seat],2);count(cx+58,discard,7)
 end
 return l
end
function B.hit(l,x,y)
 for i,p in ipairs(l.points)do if(x-p[1])^2+(y-p[2])^2<=32^2 then return'slot',i end end
 for i,b in ipairs(l.rack)do if x>=b.x and y>=b.y and x<b.x+b.w and y<b.y+b.h then return'troop',l.kinds[i]end end
end
function B.mini(M,g,F,area,position)
 local t=M.board(g);local map=maps[g.map];local node=math.max(5,math.min(18,math.min(area.w,area.h)//8));local pad=node//2+6;local points={}
 for i,p in ipairs(map.points)do points[i]={area.x+pad+p[1]*(area.w-2*pad)//1000,area.y+pad+p[2]*(area.h-2*pad)//1000}end
 for p=1,#points do for q=p+1,#points do if t.adj[p]&(1<<(q-1))~=0 then local a,b=points[p],points[q];F.nativeLine(a[1],a[2],b[1],b[2],2)end end end
 for i,p in ipairs(points)do local b=box(p[1]-node//2,p[2]-node//2,node,node);local seat=position and M.top(g,i)or 0;local r=node>=14 and t.gate[i]~=0 and 0 or 4
  if seat==1 then if r==0 then smudge.rect(b.x,b.y,node,node,true,true)else F.box(b.x,b.y,node,node,true,false,1,r)end
  elseif seat==2 then ground(F,b,r,true);frame(F,b,r,2)
  else if r==0 then smudge.rect(b.x,b.y,node,node,true,false)else F.box(b.x,b.y,node,node,true,true,1,r)end;frame(F,b,r,2)end
  if i>t.n then frame(F,box(b.x-3,b.y-3,node+6,node+6),3,2)end
 end
end
function B.preview(M,g,F,area)
 B.mini(M,g,F,box(area.x,area.y,area.w,area.h-30),true);local t=M.board(g);local medals=0;for _,r in ipairs(t.regions)do medals=medals+r[2]end
 F.native(t.name..'   '..t.goal..' TO WIN, '..medals..' ON THE BOARD',box(area.x,area.y+area.h-24,area.w,24),10,'center')
end
return B
