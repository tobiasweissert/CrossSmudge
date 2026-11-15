-- CrossPlay Insider's seats, private role cards, clock, vote and tutorial.
local F=dofile('graphics.lua');local V={hit={},focus=1,help=0,chosen=nil}
local art={};local previous
local names={C='CITIZEN',I='INSIDER',M='MASTER'}
local advice={C='FIND THE WORD. THEN FIND THE INSIDER.',I='YOU KNOW IT. DO NOT LOOK LIKE IT.',M='ANSWER YES OR NO. NOTHING ELSE.'}
function V.init()
 local ok,orientation=pcall(smudge.get_orientation);V.originalOrientation=ok and orientation or nil;pcall(smudge.set_orientation,'portrait')
 F.init();for _,name in pairs(names) do local file=name:sub(1,1)..name:sub(2):lower()
  for _,side in ipairs({48,56,96}) do art[name..side]=assert(smudge.read_file('art/role'..file..(side==96 and '' or side)..'.raw',1200)) end
 end
end
function V.exit()if V.originalOrientation then pcall(smudge.set_orientation,V.originalOrientation) end end
local function hit(b,action,value)return F.hit(V.hit,b,action,value)end
local function button(label,b,action,value,solid,size)
 F.button(label,b,solid,size);hit(b,action,value)
end
local function roleArt(role,x,y,side)smudge.bitmap(x,y,side,side,art[role..side])end
local function seat(b,n,look)
 if look=='filled' then F.round(b,10,true,false)
 elseif look=='dim' then smudge.rect_dither(b.x,b.y,b.w,b.h,false);F.round(b,10,false,false)
 else F.round(b,10,false,false,4) end
 F.center(tostring(n),b,F.fit(tostring(n),b.w-12,30),look=='filled')
end
local function menu(g,w,h)
 F.header('INSIDER',w);F.text(g.players..' PLAYERS',16,131,F.fit(g.players..' PLAYERS',w-172,30))
 local step=62
 for i=0,1 do local b=F.box(w-16-128+i*66,119,62,62);local live=i==0 and g.players>4 or i==1 and g.players<8
  F.button(i==0 and '-' or '+',b,live,30)
  if live then hit(b,'players',i==0 and -1 or 1) end
 end
 smudge.rect(16,211,w-32,3,true,true)
 local stats=g.rounds..' ROUNDS   '..g.won..' CAUGHT   '..g.lost..' AWAY   '..g.timeout..' RAN OUT'
 F.text(stats,16,225,F.fit(stats,w-32,10))
 F.center('AT THE TABLE',F.box(16,269,w-32,22),10)
 local chair=math.min(50,(w-32-6*(g.players-1))//g.players);local left=(w-(chair*g.players+6*(g.players-1)))//2
 for n=1,g.players do seat(F.box(left+(n-1)*(chair+6),299,chair,chair),n) end
 F.center('LAST 16 ROUNDS',F.box(16,389,w-32,22),10)
 local gw=math.min(398,w-72);local cell=(gw-42)//8;local gh=cell*2+6;local x,y=(w-gw)//2,431
 F.corners(F.box(x-20,y-20,gw+40,gh+40),30,3)
 for i=1,16 do local b=F.box(x+(i-1)%8*(cell+6),y+(i-1)//8*(cell+6),cell,cell);local mark=tonumber(g.recent:sub(i-(16-#g.recent),i-(16-#g.recent)))
  if i<=16-#g.recent then smudge.rect_dither(b.x,b.y,b.w,b.h,false)
  elseif mark==1 then F.round(b,4,true,false)
  else F.round(b,4,false,false);if mark==3 then smudge.line(b.x,b.y,b.x+b.w-1,b.y+b.h-1,true) end end
 end
 button('DEAL',F.box(16,h-144,w-32,62),2,nil,true,30)
 button('HOW TO PLAY',F.box(16,h-78,w-32,62),'help',1,false,20)
end
local function pass(g,M,w,h)
 F.header('PASS IT ON',w,g.seat..' OF '..g.players)
 if g.phase==1 then
  F.center('PLAYER '..g.seat,F.box(16,(h-90)//2,w-32,52),30)
  F.center('TAP TO SEE YOUR ROLE',F.box(16,(h-90)//2+60,w-32,30),20)
  local start=(w-(34*g.players+12*(g.players-1)))//2
  for n=1,g.players do F.round(F.box(start+(n-1)*46,h-50,34,34),8,n<g.seat,false,n==g.seat and 4 or 1) end
  hit(F.box(16,119,w-32,h-135),1);return
 end
 local role=g.roles:sub(g.seat,g.seat);local knows=role~='C';local card=F.box(16,119,w-32,h-209)
 F.corners(card,40,3);local block=knows and 326 or 260;local y=card.y+(card.h-block)//2
 roleArt(names[role],(w-96)//2,y,96);y=y+120
 F.center(names[role],F.box(16,y,w-32,44),30);y=y+44
 if knows then
  F.center('THE WORD IS',F.box(16,y+22,w-32,22),10)
  F.block(M.word(g.word),F.box(36,y+48,w-72,46),30,1);y=y+118
 else y=y+20 end
 F.block(advice[role],F.box(46,y,w-92,knows and 44 or 76),knows and 10 or 20,2)
 button(g.seat==g.players and 'START THE CLOCK' or 'PASS TO PLAYER '..(g.seat+1),F.box(16,h-78,w-32,62),1,nil,true)
end
local function trough(b,left)
 F.round(b,10,false,false,4);local inner=F.box(b.x+6,b.y+6,b.w-12,b.h-12)
 if left>0 then F.round(F.box(inner.x,inner.y,inner.w*left//10,inner.h),6,true,false)
  for n=1,left-1 do smudge.rect(inner.x+inner.w*n//10,inner.y,3,inner.h,true,false) end
 end
end
local function questions(g,w,h)
 F.header(g.phase==4 and 'PAUSED' or 'QUESTIONS',w,g.players..' PLAYERS')
 local block=220;local y=119+(h-119-16-74-block)//2;local master=g.roles:find('M',1,true)
 F.center('PLAYER '..master..' ANSWERS',F.box(16,y,w-32,30),20);y=y+50
 local seconds=math.ceil(g.left/5000)*5
 F.center((seconds//60)..':'..string.format('%02d',seconds%60),F.box(16,y,w-32,60),30);y=y+76
 trough(F.box(16,y,w-32,46),math.ceil(g.left/30000));y=y+72
 F.center('ASK ANYTHING. YES OR NO ONLY.',F.box(16,y,w-32,22),10)
 button(g.phase==4 and 'RESUME QUESTIONS' or 'WE SAID THE WORD',F.box(16,h-78,w-32,62),1,nil,true)
 if g.phase==3 then hit(F.box(16,119,w-32,76),2) end
end
local function vote(g,w,h)
 F.header('THE VOTE',w);F.center('WHO WAS THE INSIDER?',F.box(16,119,w-32,30),20)
 local chip=(w-32-42)//4;local master=g.roles:find('M',1,true)
 for n=1,g.players do local b=F.box(16+(n-1)%4*(chip+14),171+(n-1)//4*(chip+14),chip,chip)
  seat(b,n,n==master and 'dim' or n==V.chosen and 'filled' or 'plain')
  if n==master then F.center('MASTER',F.box(b.x,b.y+b.h-24,b.w,20),10) else hit(b,'choose',n) end
 end
 button('NOBODY WAS',F.box(16,h-190,w-32,62),'choose',0,V.chosen==0)
 F.center('SOMETIMES THE ROLE WAS NEVER DEALT.',F.box(16,h-124,w-32,30),10)
 local label=V.chosen==nil and 'CHOOSE SOMEBODY' or V.chosen==0 and 'SAY NOBODY' or 'ACCUSE PLAYER '..V.chosen
 F.button(label,F.box(16,h-78,w-32,62),V.chosen~=nil,20)
 if V.chosen~=nil then hit(F.box(16,h-78,w-32,62),'accuse',V.chosen) end
end
local function result(g,M,w,h)
 F.header('INSIDER',w);local insider=g.roles:find('I',1,true);local verdict
 if g.result==1 then verdict=insider and 'CAUGHT' or 'NOBODY, AND YOU KNEW IT'
 elseif g.result==2 then verdict=insider and 'THEY GOT AWAY' or 'THERE WAS NOBODY' else verdict='TIME UP' end
 F.round(F.box(16,119,w-32,84),20,true,false);F.block(verdict,F.box(28,119,w-56,84),30,2,true)
 F.center(insider and 'THE INSIDER WAS' or 'THERE WAS NO INSIDER.',F.box(16,249,w-32,24),10)
 if insider then seat(F.box((w-96)//2,283,96,96),insider,'filled')
 else F.center('THE ROLE WAS NEVER DEALT.',F.box(16,300,w-32,40),20) end
 if g.result==2 and V.accused~=nil then F.center(V.accused==0 and 'YOU SAID NOBODY' or 'YOU ACCUSED PLAYER '..V.accused,F.box(16,399,w-32,24),10) end
 F.center('THE WORD WAS',F.box(16,519,w-32,24),10);F.block(M.word(g.word),F.box(16,549,w-32,46),30,1)
 button('DEAL AGAIN',F.box(16,h-144,w-32,62),1,nil,true,30)
 button('BACK TO THE MENU',F.box(16,h-78,w-32,62),'menu')
end
local helpText={
 {'THE ROLES','THE MASTER AND THE INSIDER BOTH KNOW A SECRET WORD. THE CITIZENS KNOW NOTHING AT ALL.'},
 {'THE DEAL','PASS IT ROUND. EACH OF YOU LOOKS AT YOUR OWN ROLE, THEN HANDS IT ON.'},
 {'THE QUESTIONS','ASK THE MASTER YES-OR-NO QUESTIONS. YOU HAVE FIVE MINUTES TO SAY THE WORD OUT LOUD.'},
 {'THE VOTE','FOUND IT IN TIME? NOW POINT AT WHOEVER YOU THINK THE INSIDER WAS.'},
 {'THE TWIST','NOBODY SEES WHICH ONE. IF IT WAS THE INSIDER, THERE IS NO INSIDER -- SO NOBODY IS AN ANSWER YOU CAN VOTE FOR.'}}
local function help(w,h)
 F.header('HOW TO PLAY',w,V.help..' OF 5');local page=helpText[V.help];F.center(page[1],F.box(16,119,w-32,46),30)
 if V.help==1 then
  F.center('IN A GAME OF FIVE',F.box(16,171,w-32,22),14)
  local width=math.min(138,(w-64)//3);local x=(w-(width*3+32))//2
  for n,role in ipairs({'MASTER','INSIDER','CITIZEN'}) do local b=F.box(x+(n-1)*(width+16),205,width,190);F.round(b,8,false,false,3)
   roleArt(role,b.x+(width-56)//2,b.y+16,56);F.center(role,F.box(b.x,b.y+82,width,24),14);F.center(n==3 and '3' or '1',F.box(b.x,b.y+116,width,44),30)
  end
 elseif V.help==2 then
  local width=math.min(96,(w-120)//3);local x=(w-(width*3+88))//2
  for n=0,2 do local b=F.box(x+n*(width+44),249,width,128);F.round(b,6,false,false,3);smudge.rect_dither(b.x+8,b.y+10,b.w-16,b.h-26,false)
   if n<2 then F.center('>',F.box(b.x+b.w+8,289,28,44),30) end
  end
 elseif V.help==3 then F.center('? ? ?',F.box(16,219,w-32,60),30);trough(F.box(16,309,w-32,60),6)
 elseif V.help==4 then
  local size=(w-64)//5;for n=1,5 do seat(F.box(16+(n-1)*(size+8),269,size,size),n,n==4 and 'filled' or 'plain') end
 else
  F.center('EVERY ROLE BUT THE MASTER',F.box(16,169,w-32,22),14)
  local width=math.min(76,(w-80)//5);local x=(w-(width*5+48))//2
  for n=1,5 do local b=F.box(x+(n-1)*(width+12),197,width,104);F.round(b,8,false,false,3);roleArt(n==2 and 'INSIDER' or 'CITIZEN',b.x+(width-48)//2,b.y+24,48) end
  F.center('V',F.box(16,313,w-32,34),20);F.center('ONE GOES BACK, FACE DOWN',F.box(16,355,w-32,22),14)
  local b=F.box((w-width)//2,385,width,104);smudge.rect_dither(b.x,b.y,b.w,b.h,false);F.round(b,8,false,false,3);F.center('?',b,30)
 end
 local captionY=V.help==1 and 419 or V.help==5 and 513 or 499
 F.block(page[2],F.box(16,captionY,w-32,math.max(80,h-90-captionY)),20,4)
 F.center(V.help==5 and 'TAP TO FINISH' or 'TAP TO CONTINUE',F.box(16,h-82,w-32,22),14)
 F.pips(V.hit,w,h-54,5,V.help,'help-page');hit(F.box(16,119,w-32,h-210),'help-next')
end
function V.draw(g,M,notice)
 if previous~=g.phase then if previous==2 or g.phase==6 then smudge.full_refresh() end;V.focus=1;V.chosen=nil;previous=g.phase end
 local w,h=smudge.get_bounds();V.hit={};smudge.clear()
 if V.help>0 then help(w,h)
 elseif g.phase==0 then menu(g,w,h) elseif g.phase==1 or g.phase==2 then pass(g,M,w,h)
 elseif g.phase==3 or g.phase==4 then questions(g,w,h) elseif g.phase==5 then vote(g,w,h) else result(g,M,w,h) end
 if g.phase==0 and V.help==0 and V.focus==1 then for n,hit in ipairs(V.hit) do if hit.action==2 then V.focus=n;break end end end
 if V.keyboard and V.hit[V.focus] then F.rect(V.hit[V.focus].box,false,false,3) end
 if notice~='' then F.block(notice,F.box(16,84,w-32,30),14,1) end
end
function V.act(g,M,action,value)
 if type(action)=='number' then local starts=action==1 and g.phase==2 and g.seat==g.players;local result=M.act(g,action);if starts then M.act(g,1) end;return result end
 if action=='players' then local target=math.max(4,math.min(8,g.players+value));while g.players~=target do M.act(g,1) end
 elseif action=='help' then V.help=1
 elseif action=='help-next' then V.help=V.help==5 and 0 or V.help+1
 elseif action=='help-page' then V.help=value
 elseif action=='choose' then V.chosen=value
 elseif action=='accuse' and value~=nil then V.accused=value;M.act(g,value+1)
 elseif action=='menu' then g.phase=0;g.word=0;g.roles='';g.seat=1;g.result=0;V.help=0 end
end
function V.back(g)
 if V.help>0 then V.help=0;return true end
 return false
end
function V.key(g,b)
 if V.help>0 then
  if b=='confirm' or b=='down' or b=='right' or b=='page_forward' then return {action='help-next'} end
  if b=='up' or b=='left' or b=='page_back' then return {action='help-page',value=V.help==1 and 5 or V.help-1} end
 end
 if g.phase==5 then
  if b=='confirm' and V.chosen~=nil then return {action='accuse',value=V.chosen} end
  if b=='up' or b=='left' or b=='page_back' or b=='down' or b=='right' or b=='page_forward' then
   local choices={0};local master=g.roles:find('M',1,true);for seat=1,g.players do if seat~=master then choices[#choices+1]=seat end end
   local at=0;for i,value in ipairs(choices) do if value==V.chosen then at=i;break end end
   local delta=(b=='up' or b=='left' or b=='page_back') and -1 or 1;return {action='choose',value=choices[(at-1+delta)%#choices+1]}
  end
 end
 if b=='confirm' then return V.hit[V.focus] end
 if b=='up' or b=='left' or b=='page_back' then V.focus=(V.focus-2)%math.max(1,#V.hit)+1
 elseif b=='down' or b=='right' or b=='page_forward' then V.focus=V.focus%math.max(1,#V.hit)+1 end
end
function V.at(x,y)return F.at(V.hit,x,y) end
function V.clock(g)return g.left//5000 end
return V
