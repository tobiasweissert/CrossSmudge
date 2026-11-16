-- Original CrossPlay vertical ladder, private peek, clue, side-call and reveal.
local F=dofile('graphics.lua')
local V={hit={},focus=1,screen='menu',peek=false,touchPeek=false,everRevealed=false,nudge=false,clue=false}
local art={};local previous
function V.init()
 F.init();art.look=assert(smudge.read_file('art/look.raw',1800));art.pass=assert(smudge.read_file('art/pass.raw',1152))
 local ok,orientation=pcall(smudge.get_orientation);V.originalOrientation=ok and orientation or nil;pcall(smudge.set_orientation,'portrait')
end
function V.exit()V.peek=false;V.touchPeek=false;if V.originalOrientation then pcall(smudge.set_orientation,V.originalOrientation) end end
local function hit(b,action,value)return F.hit(V.hit,b,action,value)end
local function button(label,b,action,value,solid,size)F.button(label,b,solid,size or F.fit(label,b.w-24,30));hit(b,action,value)end
local function pair(M,id)local top,bottom=M.word(id):match('^(.-) / (.*)$');return top or '',bottom or '' end
local function tag(g,w)
 local label=g.round==0 and 'PRACTICE' or 'ROUND '..(g.phase==5 and g.round or g.round+1)
 F.text(label,w-16-F.width(label,14),8,14)
end
local function geometry(w,h,top,bottom)
 local lock=F.box(16,h-78,w-32,62);local topWord=F.box(16,16,w-32,63);local bottomWord=F.box(16,bottom or lock.y-73,w-32,63)
 local boardTop=top or 97;local slot=math.max(3,(bottomWord.y-10-boardTop)//20);local bw=w*5//16
 return {top=topWord,bottom=bottomWord,board=F.box(64,boardTop,bw,slot*20),right=F.box(64+bw+26,boardTop,w-16-(64+bw+26),slot*20),slot=slot,lock=F.box(64+bw+26,h-78,math.max(62,w-80-(64+bw+26)),62)}
end
local function sy(l,n)return l.board.y+(20-n)*l.slot end
local function scale(l)
 for n=1,20 do local major=n%5==0;local inset=major and 0 or l.board.w//3;local y=sy(l,n)+l.slot//2-1
  smudge.rect(l.board.x+inset,y,l.board.w-inset,3,true,true)
  if major then local text=tostring(n);F.text(text,l.board.x-20-F.width(text,14),sy(l,n)+(l.slot-F.cap[14])//2,14) end
 end
end
local function band(l,target)
 local lo,hi=math.max(1,target-2),math.min(20,target+2);local top=sy(l,hi)-5
 F.corners(F.box(l.board.x-18,top,l.board.w+36,(hi-lo+1)*l.slot+10),14,4)
 smudge.rect(l.board.x+6,sy(l,target)+2,l.board.w-12,l.slot-4,true,true)
end
local function marker(l,guess,reveal)
 if reveal then local b=F.box(l.board.x-10,sy(l,guess)-3,l.board.w+20,l.slot+6)
  F.rect(F.box(b.x,b.y-3,b.w,b.h+6),true,true);F.rect(b,false,false,4)
 else smudge.rect(l.board.x-14,sy(l,guess)+l.slot//2-5,l.board.w+28,10,true,true) end
end
local function pairSize(top,bottom,width)return F.width(F.clean(top),30)<=width and F.width(F.clean(bottom),30)<=width and 30 or 14 end
local function poles(top,bottom,l,largest)
 local size=largest==14 and 14 or pairSize(top,bottom,l.top.w)
 F.text(top,l.top.x,l.top.y+(l.top.h-F.cap[size])//2,size);F.text(bottom,l.bottom.x,l.bottom.y+(l.bottom.h-F.cap[size])//2,size)
 smudge.rect(l.top.x,l.top.y+l.top.h+4,l.top.w,3,true,true)
end
local function scoring(x,y,w)
 F.text('POINTS FOR THE ROUND',x,y,14)
 for n,row in ipairs({{'EXACT','5'},{'ONE OFF','3'},{'TWO OFF','1'},{'FURTHER OFF','0'}}) do
  F.text(row[1],x,y+n*29,14);F.text(row[2],x+w-F.width(row[2],14),y+n*29,14)
 end
end
local function menu(g,w,h)
 F.text('WAVELENGTH',16,27,F.fit('WAVELENGTH',w-32,30))
 local state=g.round>0 and g.total..' POINTS IN '..math.max(0,g.round-1)..' ROUNDS' or 'NO SESSION RUNNING. TAP START.'
 F.text(state,16,150,14);smudge.rect(16,190,w-32,3,true,true)
 F.text('THIS SESSION',16,208,14);F.text(g.total..' POINTS',w-16-F.width(g.total..' POINTS',14),208,14)
 F.text('SCORED ROUNDS',16,244,14);F.text(tostring(math.max(0,g.round-1)),w-16-F.width(tostring(math.max(0,g.round-1)),14),244,14)
 -- This core preserves session totals, not the source lifetime histogram.
 -- Do not invent a historical distribution from the last round's target.
 if g.round>0 then
  local box=F.box(16,302,w-32,170);F.corners(box,22,3)
  F.center('LAST ROUND',F.box(16,318,w-32,30),14)
  F.center('+'..g.points,F.box(16,350,w-32,86),44)
 else F.center('ROUND ONE IS PRACTICE',F.box(16,300,w-32,170),20) end
 local y=math.min(530,h-270)
 button(g.round>0 and 'PLAY ROUND '..(g.round+1) or 'START',F.box(16,y,w-32,62),2,nil,true)
 button('HOW TO PLAY',F.box(16,y+78,w-32,54),'help',nil,false,20)
 if g.round>0 then button('SEE THE SCORE SO FAR',F.box(16,y+140,w-32,54),'summary',nil,false,20)
 else F.button('NO SCORE YET',F.box(16,y+140,w-32,54),false,20) end
 if g.phase==0 or g.phase==5 then button(g.mode==1 and 'CO-OP' or 'TEAMS',F.box(16,h-60,w-32,44),'mode',nil,false,14) end
 hit(F.box(16,14,w-32,103),2)
end
local helpLines={'THE CLUE-GIVER SEES A HIDDEN','NUMBER FROM 1 TO 20, AND SAYS','ONE CLUE THAT SITS THERE.','','THE GUESSERS MOVE THE GUESS,','AND LOCK IT IN.','','3 OR MORE PLAYERS. ONE SCORE','FOR THE WHOLE TABLE. THE DEVICE','PASSES LEFT EACH ROUND.','ROUND ONE IS PRACTICE.','','GUESS WHERE THEIR WORDS SIT, NOT','WHAT THEY REALLY THINK.'}
local function help(w,h)
 F.text('HOW TO PLAY',16,27,30);smudge.rect(16,88,w-32,3,true,true)
 local step=math.min(29,(h-290)//#helpLines)
 for n,line in ipairs(helpLines) do F.text(line,16,104+(n-1)*step,14) end
 local y=104+#helpLines*step+19;smudge.rect(16,y-14,w-32,3,true,true);scoring(16,y,w-32)
 button('BACK',geometry(w,h).lock,'menu',nil,false,14)
end
local function pick(g,M,w,h)
 tag(g,w);F.text('PICK ONE',16,27,30);F.text('THE NUMBER IS NOT DRAWN YET.',16,91,14)
 local count=g.b>0 and 2 or 1;local cardh=math.min(250,(h-220-(count-1)*24)//count)
 local all={pair(M,g.a)};local btop,bbottom='', ''
 if g.b>0 then btop,bbottom=pair(M,g.b);all[#all+1]=btop;all[#all+1]=bbottom end
 local size=30;for _,word in ipairs(all) do if F.width(F.clean(word),30)>w-48 then size=14 end end
 for n=1,count do local top,bottom=pair(M,n==1 and g.a or g.b);local b=F.box(16,127+(n-1)*(cardh+24),w-32,cardh);F.rect(b,false,false,4)
  F.text(top,24,b.y+22,size);F.text(bottom,24,b.y+b.h-22-F.cap[size],size)
  for d=0,6 do smudge.rect(38+d*26,b.y+b.h//2-2,15,4,true,true) end;hit(b,n)
 end
 F.center(count==1 and 'ONE LEFT IN THE DECK' or 'TAP ONE TO CHOOSE IT',F.box(16,h-100,w-32,29),14)
end
local function pass(g,w,h)
 F.text('CLUE-GIVER ONLY',16,27,F.fit('CLUE-GIVER ONLY',w-32,30))
 F.block('PASS PRIVATELY TO THE CLUE-GIVER.',F.box(16,h//2-140,w-32,100),30,2)
 smudge.bitmap((w-96)//2,h//2-10,96,96,art.pass)
 button('READY',F.box(16,h-78,w-32,62),1,nil,true,20)
end
local function peek(g,M,w,h)
 local top,bottom=pair(M,g.pair);local l=geometry(w,h,96,h-213);poles(top,bottom,l)
 scale(l)
 if V.peek then band(l,g.target);F.center(tostring(g.target),F.box(l.right.x,l.right.y,l.right.w,133),64)
  F.center('THE NUMBER',F.box(l.right.x,l.right.y+133,l.right.w,29),14)
 else smudge.bitmap(l.right.x+(l.right.w-120)//2,l.right.y+6,120,120,art.look) end
 for n,line in ipairs({'SAY ONE THING','THAT SITS','EXACTLY THERE','','TWO EITHER SIDE','STILL SCORES','','ONLY ONE.','DO NOT ADD TO IT.'}) do
  F.center(line,F.box(l.right.x,l.right.y+190+(n-1)*29,l.right.w,29),14)
 end
 local pad=F.box(16,h-142,w-32,58)
 button(V.peek and 'KEEP HOLDING' or V.nudge and 'HOLD IT DOWN TO SEE' or 'HOLD TO SEE THE NUMBER',pad,'peek',nil,false,14)
 if V.everRevealed then button('I HAVE MY CLUE',F.box(16,h-74,w-32,58),'clue',nil,false,20)
 else F.center('HOLD THE BAR ABOVE TO SEE IT',F.box(16,h-74,w-32,58),14) end
end
local function clue(g,M,w,h)
 local top,bottom=pair(M,g.pair);tag(g,w)
 F.center('SAY IT OUT LOUD',F.box(16,14,w-32,63),30)
 F.center('THE NUMBER IS HIDDEN NOW.',F.box(16,78,w-32,29),14);F.center('EVERYONE CAN LOOK.',F.box(16,107,w-32,29),14)
 smudge.rect(60,140,w-120,3,true,true)
 local size=pairSize(top,bottom,w-32);F.center(top,F.box(16,160,w-32,63),size);F.center(bottom,F.box(16,470,w-32,63),size)
 for y=239,437,22 do smudge.rect(w//2-4,y,8,10,true,true) end
 smudge.rect(60,580,w-120,3,true,true)
 for n,line in ipairs({'ONE CLUE. TAKE YOUR TIME.','REPEAT IT. DO NOT ADD TO IT.','THEN HANDS OFF THE DEVICE.'}) do F.center(line,F.box(16,598+(n-1)*32,w-32,29),14) end
 button('PUT IT ON THE TABLE',F.box(16,h-78,w-32,62),'dial',nil,false,20)
end
local function dial(g,M,w,h)
 tag(g,w);local top,bottom=pair(M,g.pair);local l=geometry(w,h);V.ladder=l;poles(top,bottom,l);scale(l);marker(l,g.guess)
 F.center(tostring(g.guess),F.box(l.right.x,l.right.y,l.right.w,133),64)
 for n,line in ipairs({'ONE GUESS','FOR THE TABLE','','TAP THE SCALE','TO MOVE THE BAR','SIDE BUTTONS','MOVE IT ONE','','GUESSERS ONLY','','BACK PAUSES'}) do F.center(line,F.box(l.right.x,l.right.y+139+(n-1)*29,l.right.w,29),14) end
 button('LOCK IT IN',l.lock,3,nil,false,14)
 local area=F.box(l.board.x-24,l.board.y-l.slot,l.board.w+37,l.board.h+l.slot*2)
 hit(area,'step-at')
end
local function call(g,M,w,h)
 local top,bottom=pair(M,g.pair);F.center('YOU LOCKED '..g.guess,F.box(16,14,w-32,63),30)
 F.center('SIDE CALL: IS THE NUMBER NEARER',F.box(16,84,w-32,29),14)
 local one=g.guess==1 or g.guess==20;local size=pairSize(top,bottom,w-56)
 if one then
  F.center('NOTHING IS '..(g.guess==20 and 'ABOVE ' or 'BELOW ')..g.guess..'.',F.box(16,140,w-32,29),14)
  local text=g.guess==20 and bottom or top;local action=g.guess==20 and 2 or 1
  button(text,F.box(16,200,w-32,260),action,nil,false,size)
  F.center('THE ONLY WAY LEFT.',F.box(16,500,w-32,29),14)
 else
  button(top,F.box(16,140,w-32,200),1,nil,false,size)
  F.center(tostring(g.guess),F.box(16,360,w-32,63),30)
  button(bottom,F.box(16,450,w-32,200),2,nil,false,size)
  F.center('THE CLUE-GIVER STAYS OUT OF THIS.',F.box(16,668,w-32,29),14)
 end
 if g.round==0 then F.center('PRACTICE ROUND. NOTHING SCORES.',F.box(16,h-96,w-32,29),14) end
end
local function reveal(g,M,w,h)
 local top,bottom=pair(M,g.pair);local distance=math.abs(g.guess-g.target);local verdict=distance==0 and 'EXACT' or distance==1 and 'ONE OFF' or distance==2 and 'TWO OFF' or distance..' OFF'
 F.rect(F.box(16,16,w-32,76),true,false);F.center(g.round==1 and 'NO SCORE' or verdict,F.box(16,16,w-32,76),30,true)
 local l=geometry(w,h,150);poles(top,bottom,{top=F.box(16,102,w-32,29),bottom=l.bottom},14);scale(l);band(l,g.target);marker(l,g.guess,true)
 F.center('+'..g.points,F.box(l.right.x,150,l.right.w,133),64)
 local rows={g.round==1 and 'PRACTICE ROUND' or 'THIS ROUND','GUESS  '..g.guess,'NUMBER '..g.target}
 if g.mode==2 then rows[#rows+1]='SIDE CALL';rows[#rows+1]=distance==0 and 'NOT NEEDED' or (g.target>g.guess and g.call==1 or g.target<g.guess and g.call==2) and 'RIGHT' or 'WRONG' end
 rows[#rows+1]='ROUND '..g.round;rows[#rows+1]=g.total..' POINTS'
 for n,line in ipairs(rows) do F.center(line,F.box(l.right.x,283+(n-1)*29,l.right.w,29),14) end
 button('NEXT ROUND',F.box(16,h-160,w-32,62),1,nil,false,20)
end
local function pause(g,w,h)
 F.text('PAUSED',16,27,30);F.center(g.total..' POINTS',F.box(16,140,w-32,133),64)
 scoring(16,320,w-32)
 button('KEEP PLAYING',F.box(16,h-160,w-32,62),'resume',nil,false,20)
 button('BACK TO THE MENU',F.box(16,h-82,w-32,62),'menu',nil,false,20)
end
local function summary(g,w,h)
 F.text('THE SCORE',16,27,30);F.center(tostring(g.total),F.box(16,150,w-32,140),64)
 F.center('POINTS IN '..math.max(0,g.round-1)..' SCORED ROUNDS',F.box(16,310,w-32,44),20)
 F.center('THE FIRST ROUND WAS PRACTICE.',F.box(16,390,w-32,29),14)
 button('KEEP PLAYING',F.box(16,h-160,w-32,62),'menu',nil,false,20)
 button('START A NEW SESSION',F.box(16,h-82,w-32,62),'fresh',nil,false,20)
end
function V.draw(g,M,notice)
 if previous~=g.phase then
  V.focus=1;V.peek=false;V.touchPeek=false;V.buttonHeld=false;V.everRevealed=false;V.nudge=false
  if g.phase~=3 then V.clue=false end
  previous=g.phase
 end
 local w,h=smudge.get_bounds();V.hit={};smudge.clear()
 if V.screen=='front' then menu(g,w,h) elseif V.screen=='help' then help(w,h) elseif V.screen=='pause' then pause(g,w,h) elseif V.screen=='summary' then summary(g,w,h)
 elseif g.phase==0 then menu(g,w,h) elseif g.phase==1 then pick(g,M,w,h) elseif g.phase==6 then pass(g,w,h)
 elseif g.phase==2 then peek(g,M,w,h) elseif g.phase==3 and V.clue then clue(g,M,w,h)
 elseif g.phase==3 then dial(g,M,w,h) elseif g.phase==4 then call(g,M,w,h) else reveal(g,M,w,h) end
 if V.keyboard and V.hit[V.focus] and (g.phase==0 or V.screen~='menu') then F.rect(V.hit[V.focus].box,false,false,3) end
 if notice~='' then F.block(notice,F.box(16,h-34,w-32,30),14,1) end
end
function V.act(g,M,action,value)
 if type(action)=='number' then
  if V.screen=='front' then V.screen='menu';if g.phase==3 or g.phase==4 then return end end
  return M.act(g,action)
 end
 if action=='help' then V.screen='help'
 elseif action=='menu' then V.screen=g.phase==0 and 'menu' or 'front'
 elseif action=='mode' and (g.phase==0 or g.phase==5) then g.mode=3-g.mode
 elseif action=='summary' then V.screen='summary'
 elseif action=='resume' then V.screen='menu'
 elseif action=='fresh' then g.phase=0;g.round=0;g.total=0;g.points=0;g.call=0;V.screen='menu'
 elseif action=='peek' then V.nudge=true
 elseif action=='clue' and V.everRevealed then V.peek=false;V.touchPeek=false;M.act(g,1);V.clue=true
 elseif action=='dial' then V.clue=false
 elseif action=='step-at' then local target=math.max(1,math.min(20,20-(value-V.ladder.board.y)//V.ladder.slot));while g.guess~=target do M.act(g,g.guess<target and 1 or 2) end end
end
function V.back(g)
 if V.peek then smudge.full_refresh() end;V.peek=false;V.touchPeek=false
 if V.screen=='front' then return false end
 if V.screen~='menu' then V.screen=(V.screen=='pause' or g.phase==0) and 'menu' or 'front';return true end
 if g.phase==3 or g.phase==4 then V.screen='pause';return true end
 if g.phase==2 then g.phase=6;return true end
 return false
end
function V.key(g,b)
 if g.phase==2 and V.screen=='menu' then
  if b=='confirm' and not V.buttonHeld then if V.everRevealed then return {action='clue'} else V.nudge=true end end
  return nil
 end
 if g.phase==3 and not V.clue and V.screen=='menu' then
  if b=='up' or b=='left' or b=='page_back' then return {action=1} elseif b=='down' or b=='right' or b=='page_forward' then return {action=2} elseif b=='confirm' then return {action=3} end
 end
 if b=='confirm' then return V.hit[V.focus] end
 if b=='up' or b=='left' or b=='page_back' then V.focus=(V.focus-2)%math.max(1,#V.hit)+1
 elseif b=='down' or b=='right' or b=='page_forward' then V.focus=V.focus%math.max(1,#V.hit)+1 end
end
function V.at(x,y)local found=F.at(V.hit,x,y);if found and found.action=='step-at' then return {action=found.action,value=y} end;return found end
function V.hold(g,x,y)
 local found=V.at(x,y)
 if g.phase==2 and V.screen=='menu' and found and found.action=='peek' then V.touchPeek=true;V.peek=true;V.everRevealed=true;return true end
 return false
end
function V.release()if V.touchPeek then V.touchPeek=false;V.peek=V.buttonHeld or false;if not V.peek then smudge.full_refresh() end;return true end;return false end
function V.update(g)
 if g.phase~=2 or V.screen~='menu' then return end
 local ok,held=pcall(smudge.is_button_down,'confirm');held=ok and held or false
 V.buttonHeld=held;local peek=V.touchPeek or held
 if peek then V.everRevealed=true end
 if peek~=V.peek then if not peek then smudge.full_refresh() end;V.peek=peek;smudge.request_update() end
end
function V.clock(g)return g.phase end
return V
