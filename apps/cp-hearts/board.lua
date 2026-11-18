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
local U=dofile('unified_ui.lua')
local function text(t,x,y,width,bold,align)U.text(x,y,U.fit(t,width,'ui10',bold),'ui10',bold,align or'left')end
local function foot(labels,ops,values,primary,enabled)
 for i,b in ipairs(U.footer(labels,primary,enabled))do if b.enabled and ops[i]then hit(b,ops[i],values and values[i])end end
end
local function prose(t,x,y,width,bottom)
 local line='';for word in t:gmatch('%S+')do local next=line==''and word or line..' '..word
 if smudge.text_width(next,'ui10')>width and line~=''then text(line,x,y,width);y=y+29;line=word else line=next end end
 if line~=''then text(line,x,y,width);y=y+29 end;return y
end
local function card(c,b,selected,dim)
 U.panel(b,false);if selected then smudge.rect(b.x,b.y,b.w,b.h,false,4,true)end
 local rank=(c-1)%13+2;local r=({[11]='J',[12]='Q',[13]='K',[14]='A'})[rank]or tostring(rank)
 U.text(b.x+5,b.y+5,r,'ui10',true)
 pip((c-1)//13+1,b.x+5,b.y+b.h-24,16)
 if dim then smudge.rect(b.x+3,b.y+35,18,2,true,true)end
end
function V.draw(M,g,notice)
 smudge.clear();V.controls={};V.cards={};local w,h=smudge.get_bounds();V.model=M;V.g=g;local m=U.metrics()
 local passing=({'LEFT','RIGHT','ACROSS','NOBODY'})[g.handno%4+1]
 U.header('HEARTS',V.help>0 and'RULES' or V.menu and'LOCAL TABLE'or g.phase==0 and'PASS '..passing or g.phase>=3 and'SCORES'or'TRICK '..math.min(13,g.number+1))
 if V.help>0 then
  local y=notice~=''and 116 or 90;for _,t in ipairs(rules[V.help])do y=prose(t,14,y,w-28,m.body_bottom)+12 end
  foot({'Menu',V.help==4 and'Got it'or'Next'},{'menu',V.help==4 and'menu'or'help'},{nil,V.help+1},2)
 elseif V.menu then
  local offset=notice~=''and 26 or 0
  text(V.saved and'TABLE WAITING'or'FOUR SEATS / LOWEST WINS',14,90+offset,w-28,true)
  text('GAMES '..V.record.played..' / WON '..V.record.won..' / BEST '..(V.record.best or 0),14,132+offset,w-28)
  text('HAND '..(g.handno+1)..' / '..(g.phase>=3 and'SCORED'or'IN PROGRESS'),14,174+offset,w-28)
  local b={x=14,y=226+offset,w=w-28,h=48};U.button('TABLE: LOCAL',b);hit(b,'local')
  if V.refusal then text(V.refusal,14,286,w-28)end
  foot({'Rules',V.confirm and'Discard it?'or'New game','Play'},{'help','new','resume'},{1},3)
 elseif g.phase>=3 then
  local moon=0;for s=1,4 do if M.taken(g,s)==26 then moon=s end end
  local top=notice~=''and 116 or 82;local rh=math.min(80,(m.body_bottom-top-48)//4)
  for s=1,4 do
   local b={x=14,y=top+(s-1)*(rh+6),w=w-28,h=rh};U.panel(b)
   text(seatnames[s],24,b.y+10,w//3,true)
   local delta=moon>0 and(s==moon and 0 or 26)or M.taken(g,s)
   text('+'..delta..' / TOTAL '..g['t'..s],w-24,b.y+10,w//2,false,'right')
  end
  foot({'Menu',g.phase==4 and'Play again'or'Next hand'},{'menu','act'},{nil,1},2)
 else
  local hand=M.hand(g,1);local rail=math.min(250,(w-40)*2//5);local top=notice~=''and 116 or 80
  local infoH=h>600 and 156 or 132;local fx=14+rail+12;local fw=w-14-fx
  for s=1,4 do
   local rh=(infoH-18)//4;local b={x=14,y=top+(s-1)*(rh+6),w=rail,h=rh};local active=g.phase==1 and g.turn==s
   U.panel(b,active);U.text(24,b.y+(rh-25)//2,seatnames[s],'ui10',true,'left',not active)
   U.text(14+rail-10,b.y+(rh-25)//2,tostring(g['t'..s]+M.taken(g,s)),'ui10',true,'right',not active)
  end
  U.panel({x=fx,y=top,w=fw,h=infoH})
  text(g.phase==0 and'PASSING '..passing or'TRICK '..math.min(13,g.number+1),fx+10,top+8,fw-20,true)
  if g.phase==0 then
   local cw=math.min(64,(fw-32)//3);local ch=math.min(78,infoH-54);local chosen={}
   for i,c in ipairs(hand)do if g.selection:find(string.char(c),1,true)then chosen[#chosen+1]={c,i}end end
   for i=1,3 do local b={x=fx+8+(i-1)*(cw+8),y=top+44,w=cw,h=ch}
    if chosen[i]then card(chosen[i][1],b);hit(b,'act',chosen[i][2])else U.panel(b);smudge.rect_dither(b.x+2,b.y+2,b.w-4,b.h-4,true)end
   end
  else
   local cw=math.min(60,(fw-40)//4);local ch=math.min(78,infoH-48)
   for i=1,#g.trick do local s=(g.leader+i-2)%4+1;local b={x=fx+8+(s-1)*(cw+6),y=top+42,w=cw,h=ch};card(g.trick:byte(i),b);V.cards[#V.cards+1]=g.trick:byte(i)end
  end
  local handTop=h-160;local instruction
  if g.phase==0 then instruction='PICK THREE TO PASS '..passing
  elseif g.phase==2 then instruction=seatnames[M.winner(g)]..' TAKES THE TRICK'
  elseif g.turn~=1 then instruction=seatnames[g.turn]..' IS THINKING'
  elseif #g.trick==0 then instruction=g.number==0 and'LEAD THE TWO OF CLUBS'or'YOUR LEAD'
  else instruction='FOLLOW '..({'CLUBS','DIAMONDS','SPADES','HEARTS'})[(g.trick:byte(1)-1)//13+1]end
  text(V.refusal or instruction,14,handTop-66,w-28,true)
  text(g.phase==0 and #g.selection..' OF 3' or(g.broken==1 and'HEARTS BROKEN'or'HEARTS UNBROKEN'),w-14,handTop-34,w-28,false,'right')
  if #hand>0 then
   local cw=math.min(64,(w-28)//#hand+28);local step=#hand==1 and 0 or math.min(cw+8,(w-28-cw)//(#hand-1));local x=(w-cw-step*(#hand-1))//2;local legalIndex=0
   for i,c in ipairs(hand)do
    local legal=g.phase==0 or g.turn==1 and M.legal(g,c);if legal and g.phase~=0 then legalIndex=legalIndex+1 end
    local b={x=x+(i-1)*step,y=handTop,w=cw,h=82};card(c,b,g.selection:find(string.char(c),1,true)~=nil,not legal);V.cards[#V.cards+1]=c
    hit({x=b.x,y=b.y,w=i==#hand and cw or step,h=b.h},legal and'act'or'refused',g.phase==0 and i or legalIndex)
   end
  end
  foot({'Menu',g.phase==0 and'Pass'or'Play'},{'menu',g.phase==0 and'act'or nil},{nil,#hand+1},2,{true,g.phase==0 and #g.selection==3})
 end
 if notice~=''then text(notice,14,78,w-28,true)end
 V.focus=math.max(1,math.min(V.focus,#V.controls))
end
local function route(c)
 if not c then return end
 V.controls={} -- retire contacts until completed replacement paint
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
