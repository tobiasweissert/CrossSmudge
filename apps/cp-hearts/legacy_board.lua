-- Original HeartsScreens landscape geometry and original shared deck masks.
local F=dofile('graphics.lua');local V={menu=true,help=0,focus=1,controls={},cards={}}
local masks={};local seatnames={'YOU','WEST','NORTH','EAST'};local short={'Y','W','N','E'}
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function hit(b,op,value)V.controls[#V.controls+1]={box=b,op=op,value=value}end
local function plate(text,b,op,value,fill)
 F.box(b.x,b.y,b.w,b.h,fill,false,#V.controls+1==V.focus and 3 or 1);F.label(text,b.x,b.y,b.w,b.h,20,fill);hit(b,op,value)
end
local function headerButton(text,b,op,value)
 F.box(b.x,b.y,b.w,b.h,true,true);F.box(b.x,b.y,b.w,b.h,false,false,#V.controls+1==V.focus and 3 or 1);F.label(text,b.x,b.y,b.w,b.h,20);hit(b,op,value)
end
local function header(w,title)
 smudge.rect(0,0,w,56,true,true);F.text(title,16,9,30,true);smudge.rect(0,60,w,3,true,true)
end
local function pip(suit,x,y,size)smudge.bitmap(x,y,size,size,masks[suit][size])end
local function face(c,b,visible,selected,dim)
 F.box(b.x,b.y,b.w,b.h,true,true);if dim then smudge.rect_dither(b.x+3,b.y+3,b.w-6,b.h-6,false)end;F.box(b.x,b.y,b.w,b.h,false,false,selected and 4 or 2)
 local rank=(c-1)%13+2;local r=({[11]='J',[12]='Q',[13]='K',[14]='A'})[rank]or tostring(rank);local suit=(c-1)//13+1
 F.text(r,b.x+9,b.y+9,20);pip(suit,b.x+9,b.y+48,18)
 if visible>=b.h then pip(suit,b.x+b.w-26,b.y+8,18);if b.w>=72 and b.h>=90 then pip(suit,b.x+(b.w-46)//2,b.y+b.h-57,46)end end
 if selected and visible<b.h then smudge.rect(b.x+8,b.y+70,math.max(3,visible-16),18,true,true)end
end
local rules={
 {'WHAT IT COSTS','EVERY HEART YOU TAKE COSTS ONE.','THE QUEEN OF SPADES COSTS 13.','SO 26 POINTS GO OUT EVERY HAND.','LOWEST SCORE WINS, AT 100.'},
 {'TAKING A TRICK','THE TWO OF CLUBS OPENS A HAND.','FOLLOW THE SUIT LED IF YOU CAN.','HIGHEST CARD OF THAT SUIT TAKES IT','AND LEADS THE NEXT TRICK.'},
 {'THE TWO THAT CATCH PEOPLE','YOU MAY NOT LEAD A HEART UNTIL','SOMEBODY HAS DISCARDED ONE.','ON TRICK ONE, NO HEART AND NO','QUEEN, UNLESS THAT IS ALL YOU HOLD.','A GREY CARD IS ONE THE RULES REFUSE.'},
 {'PASSING, AND THE MOON','EACH HAND YOU PASS THREE CARDS:','LEFT, RIGHT, ACROSS, THEN NOBODY.','TAKE ALL 26 AND YOU SCORE NOTHING','WHILE EVERYONE ELSE TAKES 26.','THAT IS THE MOON. IT IS RARE.'}}
function V.init(saved,record)
 F.init();V.saved=saved;V.record=record;V.menu=true;V.help=0;V.confirm=false;V.focus=1
 for i,name in ipairs({'clubSolid','diamondOutline','spadeSolid','heartOutline'})do masks[i]={};for _,n in ipairs({18,46})do masks[i][n]=assert(smudge.read_file('art/'..name..'_'..n..'.raw',400))end end
end
local function menu(M,g,w,h)
 header(w,'HEARTS');headerButton('RULES',box(w-152,8,136,40),'help',1)
 local x,y=32,85;local left=(w-56)//2;F.text(V.saved and'TABLE WAITING'or'FOUR SEATS',x,y,30);F.text(V.saved and('HAND '..(g.handno+1)..' PART PLAYED')or'AVOID THE QUEEN',x,y+56,20)
 local ry=y+116;local rh=h-100-ry;local cw=left//3
 for i,label in ipairs({'GAMES','WON','BEST'})do local bx=x+(i-1)*cw;F.box(bx,ry,cw-12,rh,false,false,1);F.label(label,bx,ry+10,cw-12,30,14);local val=i==1 and V.record.played or i==2 and V.record.won or(({[0]='-','1ST','2ND','3RD','4TH'})[V.record.best]);F.label(tostring(val),bx,ry+40,cw-12,rh-50,30)end
 local fx=x+left+40;local fw=w-32-fx;F.box(fx,y,fw,h-185,false,false,1)
 F.label('NORTH',fx,y+8,fw,30,20);F.label('WEST',fx,y+70,fw//2,40,20);F.label('EAST',fx+fw//2,y+70,fw//2,40,20)
 local cw2,ch=48,64;local by=y+math.min(170,h-280)
 for i=1,3 do local bx=fx+(fw-3*cw2-20)//2+(i-1)*(cw2+10);F.box(bx,by,cw2,ch,true,true,2);F.box(bx+7,by+7,cw2-14,ch-14,false,false,2,4);pip(4,bx+(cw2-18)//2,by+(ch-18)//2,18)end
 plate(V.saved and'RESUME'or'PLAY',box(32,h-80,math.min(236,left),68),'resume',nil,true)
 if V.saved then plate(V.confirm and'DISCARD IT?'or'NEW GAME',box(280,h-80,200,68),'new')end
 if w>=700 then F.label('LOCAL TABLE',492,h-80,w-524,68,20)end
end
local function help(w,h)
 header(w,'HOW TO PLAY');headerButton('MENU',box(w-122,8,106,40),'menu')
 local r=rules[V.help];local leading=w>=700 and 42 or 36;local top=75+math.max(0,(h-135-(#r-1)*leading-74)//2)
 F.label(r[1],16,top,w-32,56,30)
 for i=2,#r do F.label(r[i],16,top+74+(i-2)*leading,w-32,leading,20)end
 for i=1,4 do plate(tostring(i),box((w-166)//2+(i-1)*44,h-60,34,34),'help',i,V.help==i)end
end
local function scores(M,g,w,h)
 header(w,g.phase==4 and'GAME OVER'or'HAND OVER');local top,foot=79,h-80;local moon=0
 for s=1,4 do if M.taken(g,s)==26 then moon=s end end
 if moon>0 then F.box(16,top,w-32,50,true);F.label(seatnames[moon]..' SHOT THE MOON',16,top,w-32,50,20,true);top=top+62 end
 local rh=(foot-10-44-top-30)//4;local minimum=math.min(g.t1,g.t2,g.t3,g.t4);local tied=0;for s=1,4 do if g['t'..s]==minimum then tied=tied+1 end end
 for s=1,4 do local y=top+(s-1)*(rh+10);local total=g['t'..s];local win=g.phase==4 and total==minimum and tied==1;F.box(16,y,w-32,rh,win,false,s==1 and 3 or 1);F.label(seatnames[s],32,y,130,rh,20,win)
  local delta=moon>0 and(s==moon and 0 or 26)or M.taken(g,s);F.label('+'..delta,166,y,80,rh,20,win)
  if g.phase==4 then local place=1;for other=1,4 do if g['t'..other]<total then place=place+1 end end;F.label(({'1ST','2ND','3RD','4TH'})[place],266,y,w-382,rh,20,win)
  elseif w>=600 then local bw=w-382;F.box(266,y+rh//2-9,bw,18,false,false,1,9);if total>0 then smudge.rect_dither(266,y+rh//2-9,math.min(bw,math.max(26,total*bw//100)),18,true)end end
  F.label(total,w-106,y,74,rh,20,win)
 end
 local note=g.t1==minimum and(tied>1 and'LEVEL ON '..minimum or'YOU LEAD ON '..minimum)or'YOU TRAIL BY '..(g.t1-minimum)
 F.label(note,16,foot-54,w-32,44,20)
 plate(g.phase==4 and'NEW GAME'or'NEXT HAND',box(16,foot,236,68),'act',1,true);plate('MENU',box(w-152,foot,136,68),'menu')
end
function V.draw(M,g,notice)
 smudge.clear();V.controls={};V.cards={};local w,h=smudge.get_bounds();V.model=M;V.g=g
 if V.help>0 then help(w,h)elseif V.menu then menu(M,g,w,h)elseif g.phase>=3 then scores(M,g,w,h)else
  header(w,'HEARTS');headerButton('MENU',box(w-122,8,106,40),'menu')
  local hand=M.hand(g,1);local handTop=h-134;local status=handTop-38;local tableTop=75;local th=status-4-tableTop;local rail=w>=700 and 264 or 132;local fx=16+rail+16;local fw=w-fx-16
  for s=1,4 do local y=tableTop+(th-(4*48+30))//2+(s-1)*58;local active=g.turn==s and g.phase==1;F.box(16,y,rail,48,active,false,1);F.label(seatnames[s],28,y,rail-104,48,w>=700 and 20 or 10,active);F.label(g['t'..s]+M.taken(g,s),16+rail-68,y,60,48,20,active)
   if M.taken(g,s)>0 then F.label('+'..M.taken(g,s),16+rail-110,y,40,48,14,active)end
  end
  F.box(fx,tableTop,fw,th,false,false,1)
  if g.phase==0 then
   F.label('PASSING '..({'LEFT','RIGHT','ACROSS','HOLD'})[g.handno%4+1],fx+28,tableTop+14,fw-56,40,20)
   local cw=math.min(92,(fw-60)//3);local ch=math.min(122,th-80);local sx=fx+(fw-3*cw-44)//2;local chosen={}
   for i,c in ipairs(hand)do if g.selection:find(string.char(c),1,true)then chosen[#chosen+1]={c,i}end end
   for i=1,3 do local b=box(sx+(i-1)*(cw+22),tableTop+th-20-ch,cw,ch);if chosen[i]then face(chosen[i][1],b,ch);hit(b,'act',chosen[i][2]);V.cards[#V.cards+1]=chosen[i][1]else smudge.rect_dither(b.x,b.y,b.w,b.h,true);F.box(b.x,b.y,b.w,b.h,false)end end
   if #g.selection==3 then headerButton('PASS',box(w-272,8,134,40),'act',#hand+1)else F.box(w-272,8,134,40,false,true);F.label('PASS',w-272,8,134,40,20,true)end
  else
   local cw=math.min(80,(fw-64)//3);local ch=math.min(99,(th-30)//2);local cx=fx+(fw-cw)//2;local cy=tableTop+(th-ch)//2
   local places={box(cx,tableTop+th-8-ch,cw,ch),box(cx-cw-16,cy,cw,ch),box(cx,tableTop+8,cw,ch),box(cx+cw+16,cy,cw,ch)}
   local trick={};for i=1,#g.trick do trick[(g.leader+i-2)%4+1]=g.trick:byte(i)end
   for s,b in ipairs(places)do if trick[s]then face(trick[s],b,ch);V.cards[#V.cards+1]=trick[s]else smudge.rect_dither(b.x,b.y,b.w,b.h,true);F.box(b.x,b.y,b.w,b.h,false,false,g.turn==s and 4 or 1);smudge.circle(b.x+b.w//2,b.y+b.h//2,15,true,false);F.label(short[s],b.x,b.y,b.w,b.h,20)end end
   if g.phase==2 then headerButton('PLAY ON',box(w-272,8,134,40),'act',1)end
  end
  local q=g.board:sub(37,37);local seen=q=='0'or tonumber(q)>4
  F.label(g.phase==0 and(#g.selection..' OF 3 SELECTED')or g.phase==2 and(seatnames[M.winner(g)]..' TAKES THE TRICK')or(seatnames[g.turn]..' TO PLAY'),16,status,(w-32)*3//5,34,20)
  F.label((g.broken==1 and'HEARTS BROKEN'or'HEARTS CLOSED')..(seen and' / QUEEN SEEN'or''),16+(w-32)*3//5,status,(w-32)*2//5,34,14)
  if #hand>0 then local cw,ch=92,122;local step=#hand==1 and 0 or math.min(cw+10,(w-32-cw)//(#hand-1));local span=cw+step*(#hand-1);local x=(w-span)//2;local legalIndex=0
   for i,c in ipairs(hand)do local legal=g.phase==0 or g.turn==1 and M.legal(g,c);if legal and g.phase~=0 then legalIndex=legalIndex+1 end;local visible=i==#hand and cw or step;local b=box(x+(i-1)*step,handTop,cw,ch);face(c,b,step<cw and step or ch,g.selection:find(string.char(c),1,true)~=nil,not legal);V.cards[#V.cards+1]=c
    if legal then if #V.controls+1==V.focus then F.box(b.x,b.y,visible,ch,false,false,3)end;hit(box(b.x,b.y,visible,ch),'act',g.phase==0 and i or legalIndex)end
   end
  end
 end
 if notice~=''then F.label(notice,16,h-20,w-32,18,10)end
 V.focus=math.max(1,math.min(V.focus,#V.controls))
end
local function route(c)
 if not c then return end
 if c.op=='act'then return c.value elseif c.op=='resume'then V.menu=false;V.saved=true;V.confirm=false
 elseif c.op=='menu'then V.menu=true;V.help=0;V.confirm=false
 elseif c.op=='help'then V.help=c.value
 elseif c.op=='new'then if V.confirm then V.confirm=false;V.saved=true;return'new'else V.confirm=true end end
 V.focus=1
end
function V.touch(x,y)for i=#V.controls,1,-1 do local c=V.controls[i];local b=c.box;if x>=b.x and y>=b.y and x<b.x+b.w and y<b.y+b.h then V.focus=i;return route(c)end end end
function V.activate()return route(V.controls[V.focus])end
function V.button(b)if #V.controls==0 then return end;local d=(b=='up'or b=='left'or b=='page_back')and -1 or 1;V.focus=(V.focus-1+d)%#V.controls+1 end
function V.playing()return not V.menu and V.help==0 end
return V
