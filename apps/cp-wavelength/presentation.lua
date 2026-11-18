local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_presentation.lua')end
-- Original WavelengthScreens layout, using existing game state and immutable original glyphs.
local F=dofile('graphics.lua')
local V={hit={},focus=1,screen='menu',peek=false,touchPeek=false,everRevealed=false,nudge=false,clue=false,abandoned=0,record={rounds=0,points=0,buckets={0,0,0,0,0}}}
local art={};local previous
-- Controls belong to the phase and view that actually reached the display.
-- Scalar snapshots avoid allocating a token on every update/input callback.
function V.valid(g)return g and V.paintedPhase==g.phase and V.paintedScreen==V.screen and V.paintedPass==V.pendingPass and V.paintedClue==V.clue end
local function box(x,y,w,h)return F.box(x,y,w,h)end
local function caps(text,x,y,w,h,size,align,paper)F.inkText(text,box(x,y,w,h),size or 14,paper,align=='left',align=='right')end
local function fill(x,y,w,h,paper)smudge.rect(x,y,w,h,true,not paper)end
local function outline(b,n,paper)fill(b.x,b.y,b.w,n,paper);fill(b.x,b.y+b.h-n,b.w,n,paper);fill(b.x,b.y,n,b.h,paper);fill(b.x+b.w-n,b.y,n,b.h,paper)end
local function hit(b,action,value)return F.hit(V.hit,b,action,value)end
local function button(label,b,action,value,ending,disabled)
 if ending then fill(b.x,b.y,b.w,b.h,true);outline(b,4)elseif disabled then smudge.rect_dither(b.x,b.y,b.w,b.h,false);smudge.rect_dither(b.x,b.y,b.w,1,true);smudge.rect_dither(b.x,b.y+b.h-1,b.w,1,true);smudge.rect_dither(b.x,b.y,1,b.h,true);smudge.rect_dither(b.x+b.w-1,b.y,1,b.h,true)else fill(b.x,b.y,b.w,b.h)end
 F.nativeText(label,box(b.x+4,b.y+2,b.w-8,b.h-4),14,not ending and not disabled)
 if action then hit(b,action,value)end
end
local function pair(M,id)local top,bottom=M.word(id):match('^(.-) / (.*)$');return top or '',bottom or ''end
local function pairSize(top,bottom,width)return F.inkWidth(top,30)<=width and F.inkWidth(bottom,30)<=width and 30 or 14 end
local U=dofile('unified_ui.lua')
local function text(t,x,y,width,font,bold,align)
 U.text(x,y,U.fit(t,width,font or'ui10',bold),font or'ui10',bold,align or'left')
end
local function center(t,y,w,font,bold)text(t,w//2,y,w-28,font,bold,'center')end
local function foot(labels,actions,primary,values,enabled)
 for i,b in ipairs(U.footer(labels,primary,enabled))do if b.enabled and actions[i]then hit(b,actions[i],values and values[i])end end
end
local function status(g)return g.round==0 and'PRACTICE'or'ROUND '..(g.phase==5 and g.round or g.round+1)end
local function menu(g,w,h)
 U.header('WAVELENGTH',status(g));local y=90
 center(g.total..' POINTS THIS SESSION',y,w,'lexend16',true);y=y+56
 center(V.record.rounds..' ROUNDS ALL TIME',y,w);y=y+38
 center(V.record.points..' POINTS ALL TIME',y,w);y=y+38
 center(V.abandoned..' ABANDONED THIS SESSION',y,w);y=y+48
 local b={x=14,y=y,w=w-28,h=48};U.button(g.mode==1 and'CO-OP'or'TEAMS',b);hit(b,'mode')
 foot({'How to play','Score',g.phase>=2 and g.phase<=4 and'Resume'or'Start'},{'help','summary',g.phase>=2 and g.phase<=4 and'resume'or'start'},3)
end
local function help(w,h)
 U.header('WAVELENGTH','HOW TO PLAY');local lines={
 'The clue-giver privately sees a number 1–20.', 'Say one clue between the two ends.', 'Guessers move the dial, then lock it.', 'Exact: 5 / one off: 3 / two off: 1.', 'Teams: correct side call adds one point.', 'Only round zero is practice: zero points.', 'Pass left each round. Back pauses.'}
 local y=84;for _,t in ipairs(lines)do text(t,14,y,w-28);y=y+math.min(42,(h-174)//#lines)end
 foot({'Back'},{'menu'})
end
local function pass(g,w,h)
 U.header('WAVELENGTH',status(g));local room=h-78-68;local y=h>600 and 78+room//5 or 86
 center('PASS THE DEVICE',y,w,'lexend16',true)
 F.inkText('< LEFT',box(14,y+56,w-28,h>600 and 120 or 80),h>600 and 64 or 44)
 local line=y+(h>600 and 174 or 128);smudge.rect(14,line,w-28,3,true,true)
 center('Whoever takes it gives the clue.',line+20,w)
 if h>600 then F.block('EVERYONE ELSE, LOOK AWAY.',box(14,line+52,w-28,70),30,2)else center('EVERYONE ELSE, LOOK AWAY.',line+58,w,'ui10',true)end
 if h>600 then y=line+128 else y=line+104 end
 center(g.round==0 and'PRACTICE ROUND'or'ROUND '..(g.round+1)..' / '..g.total..' POINTS',y,w,'ui10',true)
 if g.round==0 then center('THIS ONE DOES NOT COUNT',y+34,w)end
 if V.justAbandoned then center(V.abandoned..' ROUNDS ABANDONED',80,w)end
 foot({'I have the device'},{'ready'},1)
end
local function pick(g,M,w,h)
 U.header('WAVELENGTH',status(g));text('PICK ONE / NUMBER STILL HIDDEN',14,80,w-28,'ui10',true)
 local count=g.b>0 and 2 or 1;local ch=(h-178)//count
 for n=1,count do local a,b=pair(M,n==1 and g.a or g.b);local r={x=14,y=116+(n-1)*ch,w=w-28,h=ch-10};U.panel(r);F.block(a..' / '..b,r,30,3);hit(r,n)end
 foot({'Back'},{'menu'})
end
local function ladder(g,M,w,h,revealed)
 local a,b=pair(M,g.pair);text('20 / '..a,14,80,w-28,'ui10',true);text('1 / '..b,14,h-106,w-28,'ui10',true)
 local slot=math.max(5,(h-240)//20);local board={x=62,y=116,w=w*5//16,h=slot*20};local right={x=board.x+board.w+32,y=116,w=w-14-board.x-board.w-32,h=board.h}
 local l={board=board,right=right,slot=slot};V.ladder=l
 for n=1,20 do local yy=board.y+(20-n)*slot
  smudge.rect(board.x,yy+slot//2,board.w,1,true,true)
  if n%5==0 then text(tostring(n),14,yy-10,40)end
 end
 local yy=board.y+(20-g.guess)*slot;smudge.rect(board.x-6,yy,board.w+12,math.max(2,slot),false,2,true)
 if revealed then local ty=board.y+(20-g.target)*slot;smudge.rect(board.x+4,ty+1,board.w-8,math.max(2,slot-2),true,true)end
 return l
end
local function peek(g,M,w,h)
 U.header('WAVELENGTH',status(g));local a,b=pair(M,g.pair)
 text(a..' / '..b,14,80,w-28,'ui10',true)
 local r=box(14,126,w-28,h-292)
 F.inkText(V.peek and tostring(g.target)or'?',r,h>600 and 64 or 44)
 text('Say one thing that sits exactly there.',14,h-164,w-28)
 local b1={x=14,y=h-116,w=w-28,h=48};U.button(V.peek and'Keep holding'or'Hold to see the number',b1,true);hit(b1,'peek')
 foot({V.everRevealed and'I have my clue'or'Hold above to see it'},{V.everRevealed and'clue'or nil},1)
end
local function clue(g,M,w,h)
 U.header('WAVELENGTH',status(g));center('SAY IT OUT LOUD',90,w,'lexend16',true)
 center('The number is hidden. Everyone can look.',144,w)
 local a,b=pair(M,g.pair);F.block(a..' / '..b,box(14,190,w-28,h-340),30,3)
 center('One clue. Repeat it; do not add to it.',h-126,w)
 foot({'Put it on the table'},{'dial'},1)
end
local function dial(g,M,w,h)
 U.header('WAVELENGTH',status(g));local l=ladder(g,M,w,h)
 F.inkText(tostring(g.guess),box(l.right.x,116,l.right.w,110),64)
 text('Tap scale to move',l.right.x,238,l.right.w)
 if h>600 then text('Back pauses',l.right.x,280,l.right.w)end
 local b={x=l.board.x-24,y=l.board.y,w=l.board.w+37,h=l.board.h};hit(b,'step-at')
 foot({'-','Lock it in','+'},{2,3,1},2)
end
local function call(g,M,w,h)
 U.header('WAVELENGTH',status(g));center('YOU LOCKED '..g.guess,80,w,'lexend16',true)
 center('Is the target nearer which end?',134,w)
 local a,b=pair(M,g.pair);local top=182;local ch=(h-286)//2
 for i,t in ipairs({a,b})do if (g.guess~=20 or i==2)and(g.guess~=1 or i==1)then
  local r={x=14,y=top+(i-1)*ch,w=w-28,h=ch-8};U.button(t,r,true);hit(r,i)
 end end
 text(g.round==0 and'Practice: nothing scores.'or'Side call: this round scores.',14,h-92,w-28)
 foot({'Pause'},{'pause'})
end
local function reveal(g,M,w,h)
 U.header('WAVELENGTH',g.round==1 and'PRACTICE'or'ROUND '..g.round)
 local l=ladder(g,M,w,h,true)
 F.inkText('+'..g.points,box(l.right.x,116,l.right.w,100),64)
 text('TARGET '..g.target,l.right.x,232,l.right.w);text('GUESS '..g.guess,l.right.x,266,l.right.w)
 text('TOTAL '..g.total,l.right.x,300,l.right.w)
 if h>600 then text(g.round==1 and'Practice: no score'or'This round scores',l.right.x,350,l.right.w)end
 foot({'Score','Next round'},{'summary','next'},2)
end
local function pause(g,w,h)
 U.header('WAVELENGTH','PAUSED');center(status(g)..' / '..g.total..' POINTS',90,w,'lexend16',true)
 center('Abandon scraps this round, not your score.',158,w)
 center('The next player gets a new number.',200,w)
 center(V.abandoned..' ROUNDS ABANDONED',254,w)
 foot({'Menu','Abandon','Resume'},{'menu','abandon','resume'},3)
end
local function summary(g,w,h)
 U.header('WAVELENGTH','SCORE');center(g.total..' POINTS',90,w,'lexend16',true)
 center(math.max(0,g.round-1)..' SCORED ROUNDS',150,w)
 center(V.record.rounds..' ROUNDS ALL TIME',194,w)
 center('Ending clears this session, not records.',V.notice~=''and 232 or 250,w)
 local r=V.record;local avg=r.rounds>0 and(r.points*10+r.rounds//2)//r.rounds or 0
 center('AVERAGE ALL TIME '..(r.rounds>0 and string.format('%d.%d',avg//10,avg%10)or'--'),V.notice~=''and 270 or 290,w)
 local scored=math.max(0,g.round-1);local sa=scored>0 and(g.total*10+scored//2)//scored or 0
 center('SESSION AVERAGE '..(scored>0 and string.format('%d.%d',sa//10,sa%10)or'--'),V.notice~=''and 306 or 326,w)
 local labels={'0','1','2','3-5','6+'};local summary={}
 for i,label in ipairs(labels)do summary[#summary+1]=label..':'..r.buckets[i]end
 center(table.concat(summary,'  '),V.notice~=''and 342 or 362,w)
 if h>600 then
  for i,label in ipairs(labels)do local y=416+(i-1)*48;local b={x=14,y=y,w=w-28,h=40};U.panel(b);text(label..' POINTS',24,y+7,w//2);text(tostring(r.buckets[i]),w-24,y+7,w//3,'ui10',true,'right')end
 end
 foot({'Back','End session','Play round'},{'menu','fresh','start'},3)
end
function V.init(g)
 V.sessionStarted=g.phase>0 or g.round>0;F.init();art.look=assert(smudge.read_file('art/look.raw',1800));art.pass92=assert(smudge.read_file('art/pass92.raw',1104));local ok,orientation=pcall(smudge.get_orientation);V.originalOrientation=ok and orientation or nil;previous=nil
end
function V.exit()if V.peek then smudge.full_refresh()end;V.peek=false;V.touchPeek=false;V.buttonHeld=false;if V.originalOrientation then pcall(smudge.set_orientation,V.originalOrientation)end end
function V.draw(g,M,notice)
 if previous~=g.phase then V.focus=1;V.peek=false;V.touchPeek=false;V.buttonHeld=false;V.everRevealed=false;V.nudge=false;if g.phase~=3 then V.clue=false end;previous=g.phase end
 local w,physicalH=smudge.get_bounds();V.notice=notice;local h=notice~=''and physicalH-40 or physicalH;V.hit={};smudge.clear();local view
 if V.screen=='front'or g.phase==0 and V.screen=='menu'and not V.pendingPass then view='menu';menu(g,w,h)
 elseif V.screen=='help'then view='help';help(w,h)elseif V.screen=='pause'then view='pause';pause(g,w,h)elseif V.screen=='summary'then view='summary';summary(g,w,h)
 elseif V.pendingPass or g.phase==6 then view='pass';pass(g,w,h)elseif g.phase==1 then view='pick';pick(g,M,w,h)elseif g.phase==2 then view='peek';peek(g,M,w,h)elseif g.phase==3 and V.clue then view='clue';clue(g,M,w,h)elseif g.phase==3 then view='dial';dial(g,M,w,h)elseif g.phase==4 then view='call';call(g,M,w,h)else view='reveal';reveal(g,M,w,h)end
 V.currentView=view;V.g=g;V.paintedPhase=g.phase;V.paintedScreen=V.screen;V.paintedPass=V.pendingPass;V.paintedClue=V.clue
 if V.keyboard and V.hit[V.focus]then outline(V.hit[V.focus].box,3)end
 if notice~=''then U.text(14,physicalH-98,U.fit(notice,w-28,'ui10'),'ui10',true)end
end
function V.act(g,M,action,value)
 if not V.valid(g)then return end
 if type(action)=='number'then return M.act(g,action)end
 if action=='start'or action=='next'then V.justAbandoned=false;V.sessionStarted=true;V.screen='menu';if g.phase==3 or g.phase==4 then return end;V.pendingPass=true
 elseif action=='ready'then V.pendingPass=false;if g.phase==6 then M.act(g,1)elseif g.phase==0 or g.phase==5 then M.deal(g)end
 elseif action=='help'then V.screen='help'
 elseif action=='menu'then V.screen=g.phase==0 and'menu'or'front'
 elseif action=='mode'and(g.phase==0 or g.phase==5)then g.mode=3-g.mode
 elseif action=='summary'then V.screen='summary'
 elseif action=='pause'then V.screen='pause'
 elseif action=='resume'then V.screen='menu'
 elseif action=='fresh'then g.phase=0;g.round=0;g.total=0;g.points=0;g.call=0;V.screen='menu';V.pendingPass=false;V.abandoned=0;V.sessionStarted=false
 elseif action=='abandon'then V.abandoned=math.min(65535,V.abandoned+1);V.justAbandoned=true;V.screen='menu';M.deal(g);V.pendingPass=true
 elseif action=='peek'then V.nudge=true
 elseif action=='clue'and V.everRevealed then V.peek=false;V.touchPeek=false;M.act(g,1);V.clue=true
 elseif action=='dial'then V.clue=false
 elseif action=='step-at'then local target=math.max(1,math.min(20,20-(value-V.ladder.board.y)//V.ladder.slot));while g.guess~=target do M.act(g,g.guess<target and 1 or 2)end end
end
function V.back(g)
 V.paintedPhase=nil
 if V.peek then smudge.full_refresh()end;V.peek=false;V.touchPeek=false;V.buttonHeld=false
 if V.screen=='front'then return false end
 if V.screen=='pause'then V.screen='menu';return true end
 if V.screen~='menu'then V.screen=g.phase==0 and'menu'or'front';return true end
 if V.pendingPass then V.pendingPass=false;V.screen=g.phase==0 and'menu'or'front';return true end
 if g.phase==1 or g.phase==6 then V.screen='front';return true end
 if g.phase==2 or g.phase==3 or g.phase==4 then V.screen='pause';return true end
 if g.phase==5 then V.screen='front';return true end
 return false
end
function V.key(g,b)
 if not V.valid(g)then return end
 if g.phase==2 and V.screen=='menu'and b=='confirm'then
  if V.hit[V.focus]and V.hit[V.focus].action=='clue'and V.everRevealed then return{action='clue'}end
  V.nudge=true;return nil
 end
 if g.phase==3 and not V.clue and V.screen=='menu'then if b=='up'or b=='left'or b=='page_back'then return{action=1}elseif b=='down'or b=='right'or b=='page_forward'then return{action=2}elseif b=='confirm'then return{action=3}end end
 if b=='confirm'then return V.hit[V.focus]end;if b=='up'or b=='left'or b=='page_back'then V.focus=(V.focus-2)%math.max(1,#V.hit)+1 elseif b=='down'or b=='right'or b=='page_forward'then V.focus=V.focus%math.max(1,#V.hit)+1 end
end
function V.at(x,y)if not V.valid(V.g)then return end;local found=F.at(V.hit,x,y);if found and found.action=='step-at'then return{action=found.action,value=y}end;return found end
function V.hold(g,x,y)local found=V.at(x,y);if g.phase==2 and V.screen=='menu'and found and found.action=='peek'then V.touchPeek=true;V.peek=true;V.everRevealed=true;V.nudge=false;return true end;return false end
function V.release()if V.touchPeek then V.touchPeek=false;V.peek=V.buttonHeld or false;if not V.peek then smudge.full_refresh()end;return true end;return false end
function V.update(g)
 if not V.valid(g)or g.phase~=2 or V.screen~='menu'then return end;local ok,held=pcall(smudge.is_button_down,'confirm');held=ok and held or false;V.buttonHeld=held;local peek=V.touchPeek or held;if peek then V.everRevealed=true;V.nudge=false end;if peek~=V.peek then if not peek then smudge.full_refresh()end;V.peek=peek;smudge.request_update()end
end
function V.clock(g)return g.phase end
return V
