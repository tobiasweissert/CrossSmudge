local identityOk,identity=pcall(smudge.get_device)
if not identityOk or identity~='sticky'then return dofile('legacy_board.lua')end
-- Original HeartsScreens landscape geometry and original shared deck masks.
local F=dofile('graphics.lua');local V={menu=true,help=0,focus=1,controls={},cards={}}
local masks={};local seatnames={'YOU','WEST','NORTH','EAST'};local short={'Y','W','N','E'}
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function hit(b,op,value)V.controls[#V.controls+1]={box=b,op=op,value=value}end
local function plate(text,b,op,value,fill)
 F.box(b.x,b.y,b.w,b.h,fill,false,#V.controls+1==V.focus and 3 or 1);F.label(text,b.x,b.y,b.w,b.h,20,fill);hit(b,op,value)
end
local function headerButton(text,b,op,value)
 F.button(text,b,false,false,false);hit(b,op,value)
end
local function header(w,title)
 smudge.rect(0,0,w,56,true,true);F.text(title,16,9,30,true);smudge.rect(0,60,w,3,true,true)
end
local function pip(suit,x,y,size)smudge.bitmap(x,y,size,size==16 and 18 or size,masks[suit][size])end
local function face(c,b,visible,selected,dim)
 F.box(b.x,b.y,b.w,b.h,true,true);if dim then F.roundDither(b.x,b.y,b.w,b.h,8,false)end;F.box(b.x,b.y,b.w,b.h,false,false,selected and 4 or 2)
 local rank=(c-1)%13+2;local r=({[11]='J',[12]='Q',[13]='K',[14]='A'})[rank]or tostring(rank);local suit=(c-1)//13+1
 F.text(r,b.x+9,b.y+9,20);if visible<b.h then pip(suit,b.x+10,b.y+48,16)end
 if visible>=b.h then pip(suit,b.x+b.w-26,b.y+8,18);if b.w>=72 and b.h>=90 then pip(suit,b.x+b.w//2-20,b.y+b.h-57,46)end end
 if selected and visible<b.h then smudge.rounded_rect(b.x+8,b.y+70,math.max(3,visible-16),18,6,true,true,1)end
end
local rules={
 {'WHAT IT COSTS','EVERY HEART YOU TAKE COSTS ONE.','THE QUEEN OF SPADES COSTS 13.','SO 26 POINTS GO OUT EVERY HAND.','LOWEST SCORE WINS, AT 100.'},
 {'TAKING A TRICK','THE TWO OF CLUBS OPENS A HAND.','FOLLOW THE SUIT LED IF YOU CAN.','HIGHEST CARD OF THAT SUIT TAKES IT','AND LEADS THE NEXT TRICK.'},
 {'THE TWO THAT CATCH PEOPLE','YOU MAY NOT LEAD A HEART UNTIL','SOMEBODY HAS DISCARDED ONE.','ON TRICK ONE, NO HEART AND NO','QUEEN, UNLESS THAT IS ALL YOU HOLD.','A GREY CARD IS ONE THE RULES REFUSE.'},
 {'PASSING, AND THE MOON','EACH HAND YOU PASS THREE CARDS:','LEFT, RIGHT, ACROSS, THEN NOBODY.','TAKE ALL 26 AND YOU SCORE NOTHING','WHILE EVERYONE ELSE TAKES 26.','THAT IS THE MOON. IT IS RARE.'}}
function V.init(saved,record)
 F.init();V.saved=saved;V.record=record;V.menu=true;V.help=0;V.confirm=false;V.focus=1
 for i,name in ipairs({'clubSolid','diamondOutline','spadeSolid','heartOutline','heartSolid'})do masks[i]={};for _,n in ipairs(i==5 and{18,46}or{16,18,46})do masks[i][n]=assert(smudge.read_file('art/'..name..'_'..n..'.raw',400))end end
end
local function menu(M,g,w,h)
 header(w,'HEARTS');headerButton('RULES',box(w-152,8,136,40),'help',1)
 F.nativeLabel(V.saved and'TABLE WAITING'or'FOUR SEATS',box(32,85,372,56),30)
 F.nativeLabel(V.saved and('HAND '..(g.handno+1)..(g.phase>=3 and' SCORED'or', PART PLAYED'))or'AVOID THE QUEEN',box(32,143,372,36),20)
 for i,label in ipairs({'GAMES','WON','BEST'})do local b=box(32+(i-1)*124,201,112,179);F.box(b.x,b.y,b.w,b.h,false,false,1)
  local value=i==1 and V.record.played or i==2 and V.record.won or ({[0]='-','1ST','2ND','3RD','4TH'})[V.record.best or 0]
  F.nativeLabel(tostring(value),box(b.x,205,b.w,137),44,'center',false,30)
  F.nativeLabel(label,box(b.x,342,b.w,30),14,'center',false,10)
 end
 F.box(444,85,324,295,true,true,3,14);F.box(444,85,324,295,false,false,3,14)
 for i,label in ipairs({'NORTH','WEST','EAST'})do local b=i==1 and box(558,101,96,40)or box(i==2 and 462 or 654,204,96,40);F.box(b.x,b.y,b.w,b.h,false,false,1,6);F.nativeLabel(label,b,14,'center',false,10)end
 for i=1,5 do local b=box(530+(i-1)*26,298,48,64);F.box(b.x,b.y,b.w,b.h,true,true,2);F.box(b.x,b.y,b.w,b.h,false,false,2);F.roundDither(b.x+7,b.y+7,34,50,4,true);F.box(b.x+7,b.y+7,34,50,false,false,2,4);if i==5 then F.box(b.x+11,b.y+19,26,26,true,true,1,6);pip(5,b.x+15,b.y+23,18)end end
 F.button('PLAY',box(32,400,236,68),true,false,true);hit(box(32,400,236,68),'resume')
 local x=280;if V.saved then F.button(V.confirm and'DISCARD IT?'or'NEW GAME',box(x,400,200,68),nil,true,false);hit(box(x,400,200,68),'new');x=492 end
 F.button('TABLE: LOCAL',box(x,400,w-32-x,68),nil,true,false);hit(box(x,400,w-32-x,68),'local')
end
local function help(w,h)
 local page=V.help;local r=rules[page];header(w,'HOW TO PLAY')
 local top=75+(420-16-75-(56+18+(#r-1)*42))//2
 F.nativeLabel(r[1],box(32,top,w-64,56),30)
 for i=2,#r do F.nativeLabel(r[i],box(32,top+74+(i-2)*42,w-64,38),20)end
 for i=1,4 do local b=box((w-166)//2+(i-1)*44,420,34,34);F.box(b.x,b.y,b.w,b.h,i==page,false,1,8);F.nativeLabel(tostring(i),b,14,'center',i==page,10);hit(b,'help',i)end
 local b=box(w-216,402,200,60);F.button(page==4 and'GOT IT'or'NEXT',b,true,false,true);hit(b,page==4 and'menu'or'help',page+1)
end
local function scores(M,g,w,h)
 header(w,g.phase==4 and'GAME OVER'or'HAND OVER');headerButton('MENU',box(w-122,8,106,40),'menu');local top,foot=79,h-80;local moon=0
 for seat=1,4 do if M.taken(g,seat)==26 then moon=seat end end
 if moon>0 then F.box(16,top,w-32,50,true);F.nativeLabel(seatnames[moon]..' SHOT THE MOON',box(16,top,w-32,50),20,'center',true);top=top+62 end
 local rh=(foot-10-44-top-30)//4;local minimum=math.min(g.t1,g.t2,g.t3,g.t4);local tied,champ,second=0,1,10000
 for seat=1,4 do if g['t'..seat]==minimum then tied=tied+1;champ=seat elseif g['t'..seat]<second then second=g['t'..seat]end end
 for seat=1,4 do local y=top+(seat-1)*(rh+10);local total=g['t'..seat];local win=g.phase==4 and total==minimum and tied==1
  F.box(16,y,w-32,rh,win,false,seat==1 and 3 or 1)
  F.nativeLabel(seatnames[seat],box(32,y,130,rh),20,'left',win)
  local delta=moon>0 and(seat==moon and 0 or 26)or M.taken(g,seat);F.nativeLabel('+'..delta,box(166,y,80,rh),20,'right',win)
  if g.phase==4 then local place=1;for other=1,4 do if g['t'..other]<total then place=place+1 end end;F.nativeLabel(({'1ST','2ND','3RD','4TH'})[place],box(266,y,w-382,rh),20,'center',win)
  else local bw=w-382;F.box(266,y+rh//2-9,bw,18,false,false,1,9);if total>0 then F.roundDither(266,y+rh//2-9,math.min(bw,math.max(26,total*bw//100)),18,9,true)end end
  F.nativeLabel(tostring(total),box(w-106,y,74,rh),20,'right',win)
 end
 local note
 if g.phase==4 then note=tied>1 and('TIED ON '..minimum)or(seatnames[champ]..(champ==1 and' WIN BY 'or' WINS BY ')..(second-minimum))
 elseif tied>1 then note=g.t1==minimum and('LEVEL ON '..minimum)or(({'','','TWO','THREE','ALL FOUR'})[tied+1]..' LEAD ON '..minimum..'. YOU ARE '..(g.t1-minimum)..' BEHIND')
 elseif champ==1 then note='YOU LEAD BY '..(second-minimum)else note=seatnames[champ]..' LEADS. YOU ARE '..(g.t1-minimum)..' BEHIND'end
 F.nativeLabel(note,box(16,top+4*rh+30+10,w-32,44),g.phase==4 and 30 or 20)
 local b=box(16,foot,280,68);F.button(g.phase==4 and'PLAY AGAIN'or'NEXT HAND',b,true,false,true);hit(b,'act',1)
 F.nativeLabel('LOWEST WINS. FIRST TO 100 ENDS IT',box(316,foot,w-332,68),14,'right',false,10)
end
function V.draw(M,g,notice)
 smudge.clear();V.controls={};V.cards={};local w,h=smudge.get_bounds();V.model=M;V.g=g
 if V.help>0 then help(w,h)elseif V.menu then menu(M,g,w,h)elseif g.phase>=3 then scores(M,g,w,h)else
  header(w,'HEARTS');headerButton('MENU',box(w-122,8,106,40),'menu')
  local hand=M.hand(g,1);local handTop=h-134;local status=handTop-38;local tableTop=75;local th=status-4-tableTop;local rail=w>=700 and 264 or 132;local fx=16+rail+16;local fw=w-fx-16
  for s=1,4 do local y=tableTop+(th-(4*48+30))//2+(s-1)*58;local active=g.turn==s and g.phase==1;local taken=M.taken(g,s);if g.phase==2 and M.winner(g)==s then for i=1,#g.trick do local c=g.trick:byte(i);taken=taken+(c==37 and 13 or c>39 and 1 or 0)end end
   F.box(16,y,rail,48,active,false,active and 0 or 1);F.nativeLabel(seatnames[s],box(28,y,rail-132,48),20,'left',active);F.nativeLabel(tostring(g['t'..s]+taken),box(16+rail-72,y,60,48),20,'right',active)
   if taken>0 then local p=box(16+rail-120,y+10,40,28);F.box(p.x,p.y,p.w,p.h,true,active,1,14);F.nativeLabel('+'..taken,p,14,'center',not active,10)end
  end
  F.box(fx,tableTop,fw,th,false,false,3,14)
  if g.phase==0 then
   F.nativeLabel('PASSING '..({'LEFT','RIGHT','ACROSS','NOBODY'})[g.handno%4+1],box(fx+28,tableTop+14,fw-56,40),20)
   local cw=math.min(92,(fw-60)//3);local ch=math.min(122,th-80);local sx=fx+(fw-3*cw-44)//2;local chosen={}
   for i,c in ipairs(hand)do if g.selection:find(string.char(c),1,true)then chosen[#chosen+1]={c,i}end end
   for i=1,3 do local b=box(sx+(i-1)*(cw+22),tableTop+th-20-ch,cw,ch);if chosen[i]then face(chosen[i][1],b,ch);hit(b,'act',chosen[i][2]);V.cards[#V.cards+1]=chosen[i][1]else F.roundDither(b.x,b.y,b.w,b.h,8,true);F.box(b.x,b.y,b.w,b.h,false)end end
   if #g.selection==3 then headerButton('PASS',box(w-272,8,134,40),'act',#hand+1)else F.button('PASS',box(w-272,8,134,40),nil,false,true)end
  else
   local cw=math.min(80,(fw-64)//3);local ch=(th-30)//2;local cx=fx+(fw-cw)//2;local cy=tableTop+(th-ch)//2
   local places={box(cx,tableTop+th-8-ch,cw,ch),box(cx-cw-16,cy,cw,ch),box(cx,tableTop+8,cw,ch),box(cx+cw+16,cy,cw,ch)}
   local trick={};for i=1,#g.trick do trick[(g.leader+i-2)%4+1]=g.trick:byte(i)end
   for s,b in ipairs(places)do if trick[s]then face(trick[s],b,ch);V.cards[#V.cards+1]=trick[s]else F.roundDither(b.x,b.y,b.w,b.h,8,true);F.box(b.x,b.y,b.w,b.h,false,false,g.turn==s and 4 or 1);local d=math.min(b.w,b.h)*3//5;local disc=box(b.x+(b.w-d)//2,b.y+(b.h-d)//2,d,d);F.box(disc.x,disc.y,d,d,true,true,1,d//2);F.box(disc.x,disc.y,d,d,false,false,1,d//2);F.nativeLabel(short[s],disc,20,'center')end end
   F.nativeLabel('TRICK '..math.min(13,g.number+1)..'/13',box(fx+18,tableTop+12,160,28),14,'left',false,10)
   if #g.trick>0 then local b=places[M.winner(g)];F.box(b.x-5,b.y-5,b.w+10,b.h+10,false,false,4,12)end
  end
  local q=g.board:sub(37,37);local seen=q=='0'or tonumber(q)>4
  local statusText,sub
  if g.phase==0 then statusText='PICK THREE TO PASS '..({'LEFT','RIGHT','ACROSS','NOBODY'})[g.handno%4+1];sub=#g.selection..' OF 3'
  elseif g.phase==2 then local pts=0;for i=1,#g.trick do local c=g.trick:byte(i);pts=pts+(c==37 and 13 or c>=40 and 1 or 0)end;local win=M.winner(g);statusText=seatnames[win]..(win==1 and' TAKE IT'or' TAKES IT')..(pts>0 and(', '..pts..(pts==1 and' POINT'or' POINTS'))or'')
  elseif g.turn~=1 then statusText=seatnames[g.turn]..' IS THINKING'
  elseif #g.trick==0 then statusText=g.number==0 and'LEAD THE TWO OF CLUBS'or'YOUR LEAD'
  else statusText='FOLLOW '..({'CLUBS','DIAMONDS','SPADES','HEARTS'})[(g.trick:byte(1)-1)//13+1]end
  sub=sub or((g.broken==1 and'HEARTS BROKEN'or'HEARTS UNBROKEN')..(seen and'   QUEEN SEEN'or'   QUEEN UNSEEN'))
  F.nativeLabel(V.refusal or statusText,box(16,status,460,34),20)
  F.nativeLabel(sub,box(476,status,308,34),14,'right',false,10)
  if #hand>0 then local cw,ch=92,122;local step=#hand==1 and 0 or math.min(cw+10,(w-32-cw)//(#hand-1));local span=cw+step*(#hand-1);local x=(w-span)//2;local legalIndex=0
   for i,c in ipairs(hand)do local legal=g.phase==0 or g.turn==1 and M.legal(g,c);if legal and g.phase~=0 then legalIndex=legalIndex+1 end;local visible=i==#hand and cw or step;local b=box(x+(i-1)*step,handTop,cw,ch);face(c,b,step<cw and step or ch,g.selection:find(string.char(c),1,true)~=nil,not legal);V.cards[#V.cards+1]=c
    if legal then hit(box(b.x,b.y,visible,ch),'act',g.phase==0 and i or legalIndex)end
   end
  end
 end
 if notice~=''then F.label(notice,16,h-20,w-32,18,10)end
 V.focus=math.max(1,math.min(V.focus,#V.controls))
end
local function route(c)
 if not c then return end
 if c.op=='refused'then local g,M=V.g,V.model;if g.turn~=1 then V.refusal='NOT YOUR TURN YET'elseif #g.trick==0 and g.number==0 then V.refusal='THE TWO OF CLUBS OPENS'elseif #g.trick>0 then local suit=(g.trick:byte(1)-1)//13;local follows=false;for _,card in ipairs(M.hand(g,1))do if(card-1)//13==suit then follows=true end end;V.refusal=follows and'YOU STILL HOLD THAT SUIT'or g.number==0 and'NO POINTS ON TRICK ONE'or'THE RULES REFUSE THAT'else V.refusal='HEARTS ARE NOT BROKEN'end
 elseif c.op=='local'then V.refusal='LOCAL TABLE: ONE OPPONENT STRENGTH'
 elseif c.op=='act'then V.refusal=nil;return c.value elseif c.op=='resume'then V.menu=false;V.saved=true;V.confirm=false
 elseif c.op=='menu'then V.menu=true;V.help=0;V.confirm=false
 elseif c.op=='local'then V.refusal='LOCAL PUBLIC-INFORMATION TABLE'
 elseif c.op=='help'then V.help=c.value
 elseif c.op=='new'then
  -- Retire the old NEW GAME contact before DISCARD IT? is painted.
  V.controls={}
  if V.confirm then V.confirm=false;V.saved=true;return'new'else V.confirm=true end end
 V.focus=1
end
function V.touch(x,y)for i=#V.controls,1,-1 do local c=V.controls[i];local b=c.box;if x>=b.x and y>=b.y and x<b.x+b.w and y<b.y+b.h then V.focus=i;return route(c)end end end
function V.activate()return route(V.controls[V.focus])end
function V.button(b)if #V.controls==0 then return end;local d=(b=='up'or b=='left'or b=='page_back')and -1 or 1;V.focus=(V.focus-1+d)%#V.controls+1 end
function V.playing()return not V.menu and V.help==0 end
return V
