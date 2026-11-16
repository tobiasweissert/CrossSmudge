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
local function tag(g,w)local label=g.round==0 and 'PRACTICE'or'ROUND '..(g.phase==5 and g.round or g.round+1);caps(label,w//2,14,w//2-16,29,14,'right')end
local function geometry(w,h,top)
 local lock=box(16,h-78,w-32,62);local word=box(16,16,w-32,63);local bottom=box(16,lock.y-73,w-32,63);local y=top or 97;local slot=(bottom.y-10-y)//20;local bw=w*5//16;local rx=64+bw+26
 return{top=word,bottom=bottom,board=box(64,y,bw,slot*20),right=box(rx,y,w-16-rx,slot*20),slot=slot,lock=box(rx,h-78,w-16-rx-64,62)}
end
local function sy(l,n)return l.board.y+(20-n)*l.slot end
local function scale(l)
 for n=1,20 do local major=n%5==0;local inset=major and 0 or l.board.w//3;local y=sy(l,n)+l.slot//2-1;fill(l.board.x+inset,y,l.board.w-inset,3)
 if major then caps(string.format('%2d',n),l.board.x-60,sy(l,n),40,l.slot,14,'right')end end
end
local function band(l,target)local lo,hi=math.max(1,target-2),math.min(20,target+2);F.corners(box(l.board.x-18,sy(l,hi)-5,l.board.w+36,(hi-lo+1)*l.slot+10),14,4);fill(l.board.x+6,sy(l,target)+2,l.board.w-12,l.slot-4)end
local function marker(l,guess,reveal)
 if reveal then local b=box(l.board.x-10,sy(l,guess)-3,l.board.w+20,l.slot+6);fill(b.x,b.y-3,b.w,b.h+6,true);outline(b,4)else fill(l.board.x-14,sy(l,guess)+l.slot//2-5,l.board.w+28,10)end
end
local function poles(top,bottom,l,size,peek)
 size=size or pairSize(top,bottom,l.top.w);caps(top,l.top.x,l.top.y,l.top.w,l.top.h,size,'left');fill(l.top.x,l.top.y+l.top.h+(peek and 5 or 4),l.top.w,3);caps(bottom,l.bottom.x,l.bottom.y,l.bottom.w,l.bottom.h,size,'left')
end
local scores={{'EXACT','5'},{'ONE OFF','3'},{'TWO OFF','1'},{'FURTHER OFF','0'}}
local function scoring(x,y,w) caps('POINTS FOR THE ROUND',x,y,w,29,14,'left');for n,row in ipairs(scores)do caps(row[1],x,y+n*29,w,29,14,'left');caps(row[2],x,y+n*29,w,29,14,'right')end end
local function ornament(x,y,w,h)
 local r=V.record;F.corners(box(x,y,w,h),14,4);local row=(h-20)//5;local peak=math.max(table.unpack(r.buckets));local max=(w-136)//19
 for i,label in ipairs({'0','1','2','3-5','6+'})do local yy=y+10+(i-1)*row;caps(label,x+12,yy,52,row,14,'left');local n=peak<=max and r.buckets[i]or(r.buckets[i]*max+peak-1)//math.max(1,peak);for k=0,n-1 do fill(x+70+k*19,yy+row//2-6,13,13)end;caps(tostring(r.buckets[i]),x+w-60,yy,46,row,14,'right')end
end
local function menu(g,w,h)
 caps('WAVELENGTH',16,14,w-32,63,30,'left');hit(box(16,14,w-32,103),'start')
 local progress=V.sessionStarted or g.phase>0 or g.round>0;local scored=math.max(0,g.round-1)
 if progress then caps('THIS SESSION',16,150,w-32,29,14,'left');caps(g.total..(g.total==1 and' POINT'or' POINTS')..' IN '..scored..(scored==1 and' ROUND'or' ROUNDS'),16,150,w-32,29,14,'right')else caps('NO SESSION RUNNING. TAP START.',16,150,w-32,29,14,'left')end
 fill(16,190,w-32,3);caps('ROUNDS ALL TIME',16,200,w-32,29,14,'left');caps(tostring(V.record.rounds),16,200,w-32,29,14,'right');caps('POINTS PER ROUND',16,229,w-32,29,14,'left');local avg=V.record.rounds>0 and (V.record.points*10+V.record.rounds//2)//V.record.rounds or 0;caps(V.record.rounds>0 and string.format('%d.%d',avg//10,avg%10)or'--',16,229,w-32,29,14,'right');caps('HOW FAR OFF, ALL TIME',16,262,w-32,29,14,'left');caps('ROUNDS',16,262,w-32,29,14,'right');ornament(16,296,w-32,210)
 caps('RECORD SINCE UI UPDATE',16,508,w-32,18,14);button(progress and'PLAY ROUND '..(g.phase==5 and g.round+1 or g.round+1)or'START',box(16,530,w-32,66),'start');button('HOW TO PLAY',box(16,612,w-32,54),'help');button(progress and'SEE THE SCORE SO FAR'or'NO SCORE YET',box(16,674,w-32,54),progress and'summary'or nil,nil,false,not progress)
 -- Existing Co-op saves remain playable; original Activity hardcoded Teams.
 button(g.mode==1 and'CO-OP'or'TEAMS',box(16,h-56,w-32,40),'mode',nil,true)
end
local helpLines={'THE CLUE-GIVER SEES A HIDDEN','NUMBER FROM 1 TO 20, AND SAYS','ONE CLUE THAT SITS THERE.','','THE GUESSERS MOVE THE GUESS,','AND LOCK IT IN.','','3 OR MORE PLAYERS. ONE SCORE','FOR THE WHOLE TABLE. THE DEVICE','PASSES LEFT EACH ROUND.','ROUND ONE IS PRACTICE.','','GUESS WHERE THEIR WORDS SIT, NOT','WHAT THEY REALLY THINK.'}
local function help(w,h)
 caps('HOW TO PLAY',16,14,w-32,63,30,'left');fill(16,88,w-32,3);for n,line in ipairs(helpLines)do if line~=''then caps(line,16,104+(n-1)*29,w-32,29,14,'left')end end;fill(16,515,w-32,3);scoring(16,529,w-32);button('BACK',geometry(w,h).lock,'menu')
end
local function pass(g,w,h)
 caps('PASS THE DEVICE',16,170,w-32,63,30);caps('LEFT',16,245,w-32,133,64);smudge.bitmap(36,265,92,92,art.pass92);fill(90,400,w-180,3);caps('WHOEVER TAKES IT GIVES THE CLUE.',16,416,w-32,29,14);caps('EVERYONE ELSE,',16,470,w-32,63,30);caps('LOOK AWAY.',16,533,w-32,63,30)
 if V.justAbandoned then caps(V.abandoned..(V.abandoned==1 and' ROUND'or' ROUNDS')..' ABANDONED SO FAR',16,120,w-32,29,14)end
 caps(g.round==0 and'PRACTICE ROUND'or'ROUND '..(g.round+1)..'   '..g.total..(g.total==1 and' POINT'or' POINTS'),16,625,w-32,63,g.round==0 and 30 or 14)
 if g.round==0 then caps('THIS ONE DOES NOT COUNT',16,690,w-32,29,14)end;button('I HAVE THE DEVICE',box(16,h-78,w-32,62),'ready')
end
local function pick(g,M,w,h)
 tag(g,w);caps('PICK ONE',16,14,w-32,63,30,'left');caps('THE NUMBER IS NOT DRAWN YET.',16,84,w-32,29,14,'left');local a,b=pair(M,g.a);local c,d=pair(M,g.b>0 and g.b or g.a);local size=30;for _,word in ipairs({a,b,c,d})do if F.inkWidth(word,30)>w-48 then size=14 end end
 for n=1,g.b>0 and 2 or 1 do local top,bottom=pair(M,n==1 and g.a or g.b);local y=120+(n-1)*274;outline(box(16,y,w-32,250),4);caps(top,24,y+22,w-48,63,size,'left');caps(bottom,24,y+165,w-48,63,size,'left');for k=0,6 do fill(38+k*26,y+123,15,4)end;hit(box(16,y,w-32,250),n)end;caps(g.b>0 and'TAP ONE TO CHOOSE IT'or'ONE LEFT IN THE DECK',16,h-100,w-32,29,14)
end
local function peek(g,M,w,h)
 local top,bottom=pair(M,g.pair);local l=geometry(w,h,96);l.top.y=14;l.bottom.y=h-213;l.slot=(l.bottom.y-10-96)//20;l.board.h=l.slot*20;l.right.h=l.board.h;poles(top,bottom,l,nil,true);scale(l)
 if V.peek then band(l,g.target);caps(string.format('%2d',g.target),l.right.x,96,l.right.w,133,64);caps('THE NUMBER',l.right.x,229,l.right.w,29,14)else smudge.bitmap(l.right.x+(l.right.w-120)//2,158,120,120,art.look)end
 for n,line in ipairs({'SAY ONE THING','THAT SITS','EXACTLY THERE','','TWO EITHER SIDE','STILL SCORES','','ONLY ONE.','DO NOT ADD TO IT.'})do if line~=''then caps(line,l.right.x,286+(n-1)*29,l.right.w,29,14)end end
 button(V.peek and'KEEP HOLDING'or V.nudge and'HOLD IT DOWN TO SEE'or'HOLD TO SEE THE NUMBER',box(16,h-142,w-32,58),'peek')
 if V.everRevealed then button('I HAVE MY CLUE',box(16,h-74,w-32,58),'clue')else caps('HOLD THE BAR ABOVE TO SEE IT',16,h-74+(58-29)//2,w-32,29,14)end
end
local function clue(g,M,w,h)
 tag(g,w);local top,bottom=pair(M,g.pair);caps('SAY IT OUT LOUD',16,14,w-32,63,30);caps('THE NUMBER IS HIDDEN NOW.',16,78,w-32,29,14);caps('EVERYONE CAN LOOK.',16,107,w-32,29,14);fill(60,140,w-120,3);local size=pairSize(top,bottom,w-32);caps(top,16,160,w-32,63,size);caps(bottom,16,470,w-32,63,size);for y=239,443,22 do fill(w//2-4,y,8,10)end;fill(60,580,w-120,3);caps('ONE CLUE. TAKE YOUR TIME.',16,598,w-32,29,14);caps('REPEAT IT. DO NOT ADD TO IT.',16,630,w-32,29,14);caps('THEN HANDS OFF THE DEVICE.',16,664,w-32,29,14);button('PUT IT ON THE TABLE',box(16,h-78,w-32,62),'dial')
end
local function endWord(word,b,action,size)
 button('',b,action);local inner=b.w-24
 if F.inkWidth(word,14)<=inner then caps(word,b.x+12,b.y+(b.h-63)//2,inner,63,size,nil,true);return end
 local best;for i=1,#word do if word:sub(i,i)==' 'and(not best or math.abs(i-1-#word//2)<math.abs(best-1-#word//2))then best=i end end
 if not best then caps(word,b.x+12,b.y+(b.h-63)//2,inner,63,14,nil,true);return end
 local y=b.y+(b.h-58)//2;caps(word:sub(1,best-1),b.x+12,y,inner,29,14,nil,true);caps(word:sub(best+1),b.x+12,y+29,inner,29,14,nil,true)
end
local function call(g,M,w,h)
 local top,bottom=pair(M,g.pair);caps('YOU LOCKED '..g.guess,16,14,w-32,63,30);caps('SIDE CALL: IS THE NUMBER NEARER',16,84,w-32,29,14)
 if g.guess==1 or g.guess==20 then caps('NOTHING IS '..(g.guess==20 and'ABOVE 'or'BELOW ')..g.guess..'.',16,140,w-32,29,14);endWord(g.guess==20 and bottom or top,box(16,200,w-32,260),g.guess==20 and 2 or 1,30);caps('THE ONLY WAY LEFT.',16,500,w-32,29,14)
 else local size=pairSize(top,bottom,w-56);endWord(top,box(16,140,w-32,200),1,size);caps(tostring(g.guess),16,360,w-32,63,30);endWord(bottom,box(16,450,w-32,200),2,size);caps('THE CLUE-GIVER STAYS OUT OF THIS.',16,668,w-32,29,14)end
 -- Narrow truthfulness correction to the original scored-call footnote.
 caps(g.round==0 and'PRACTICE ROUND. NOTHING SCORES.'or'SIDE CALL. THIS ROUND SCORES.',16,h-96,w-32,29,14)
end
local function dial(g,M,w,h)
 tag(g,w);local top,bottom=pair(M,g.pair);local l=geometry(w,h);V.ladder=l;poles(top,bottom,l);scale(l);marker(l,g.guess);caps(string.format('%2d',g.guess),l.right.x,l.right.y,l.right.w,133,64)
 local x,rw,y=l.right.x,l.right.w,l.right.y+139;caps('ONE GUESS',x,y,rw,29,14);caps('FOR THE TABLE',x,y+29,rw,29,14);fill(x+20,y+72,rw-40,3);local hint=y+88
 for _,row in ipairs({{0,'TAP THE SCALE'},{29,'TO MOVE THE BAR'},{72,'SIDE BUTTONS'},{101,'MOVE IT ONE'},{169,'GUESSERS'},{198,'ONLY'},{256,'BACK PAUSES'}})do caps(row[2],x,hint+row[1],rw,29,14)end
 button('LOCK IT IN',l.lock,3);hit(box(l.board.x-24,l.board.y-l.slot,l.board.w+37,l.board.h+l.slot*2),'step-at')
end
local distances={'EXACT','ONE OFF','TWO OFF','THREE OFF','FOUR OFF','FIVE OFF','SIX OFF','SEVEN OFF','EIGHT OFF','NINE OFF','TEN OFF','ELEVEN OFF','TWELVE OFF','THIRTEEN OFF','FOURTEEN OFF','FIFTEEN OFF','SIXTEEN OFF','SEVENTEEN OFF','EIGHTEEN OFF','NINETEEN OFF'}
local function reveal(g,M,w,h)
 local top,bottom=pair(M,g.pair);local l=geometry(w,h,150);local miss=math.abs(g.guess-g.target);local practice=g.round==1
 fill(16,16,w-32,76);caps(practice and'NO SCORE'or distances[miss+1],16,16,w-32,76,30,nil,true);caps(top,16,102,w-32,29,14,'left');scale(l);band(l,g.target);marker(l,g.guess,true);caps(bottom,l.bottom.x,l.bottom.y,l.bottom.w,l.bottom.h,14,'left')
 local x,rw=l.right.x,l.right.w;caps('+'..g.points,x,150,rw,133,64);caps(practice and'PRACTICE ROUND'or'THIS ROUND',x,283,rw,29,14)
 local sx=x+6;outline(box(sx,326,26,12),3);caps('GUESS  '..g.guess,sx+34,318,rw-40,29,14,'left');fill(sx,361,26,12);caps('NUMBER '..g.target,sx+34,353,rw-40,29,14,'left')
 if g.mode==2 then local right=g.target>g.guess and g.call==1 or g.target<g.guess and g.call==2;caps('SIDE CALL',x,388,rw,29,14);caps(miss==0 and'NOT NEEDED'or practice and(right and'RIGHT'or'WRONG')or(right and'RIGHT  +1'or'WRONG  +0'),x,417,rw,29,14)end
 if practice then caps('POINTS FOR',x,456,rw,29,14,'left');caps('THE ROUND',x,485,rw,29,14,'left');for n,row in ipairs(scores)do caps(row[1],x,485+n*29,rw,29,14,'left');caps(row[2],x,485+n*29,rw,29,14,'right')end
 else caps('ROUND '..g.round,x,456,rw,29,14);caps(g.total..(g.total==1 and' POINT'or' POINTS'),x,485,rw,29,14)end
 button('NEXT ROUND',box(16,h-160,w-32,62),'next')
end
local function pause(g,w,h)
 caps('PAUSED',16,14,w-32,63,30,'left');caps(g.round==0 and'PRACTICE ROUND'or'ROUND '..(g.round+1)..'   '..g.total..(g.total==1 and' POINT'or' POINTS'),16,88,w-32,29,14,'left');fill(16,126,w-32,3);scoring(16,150,w-32);button('RESUME THE ROUND',box(16,340,w-32,72),'resume');fill(16,440,w-32,3);for n,line in ipairs({'THIS ROUND IS SCRAPPED. THE NEXT','PLAYER GETS A NEW NUMBER, AND','THE PASS SCREEN SAYS SO.'})do caps(line,16,458+(n-1)*29,w-32,29,14,'left')end
 if V.abandoned>0 then caps(V.abandoned..(V.abandoned==1 and' ROUND'or' ROUNDS')..' ABANDONED SO FAR',16,556,w-32,29,14,'left')end;button('ABANDON THIS ROUND',box(16,h-78,w-32,62),'abandon',nil,true)
end
local function summary(g,w,h)
 local scored=math.max(0,g.round-1);caps(tostring(g.total),16,14,w-32,133,64,'left');caps(g.total==1 and'POINT'or'POINTS',16,150,w-32,63,30,'left');caps(scored>0 and'IN '..scored..' SCORED ROUND'..(scored==1 and''or'S')or'NO SCORED ROUNDS YET',16,214,w-32,29,14,'left');if V.abandoned>0 then caps(V.abandoned..' ABANDONED',16,214,w-32,29,14,'right')end;fill(16,252,w-32,3)
 if scored>0 then local avg=(g.total*10+scored//2)//scored;caps('POINTS PER ROUND',16,262,w-32,29,14,'left');caps('THIS SESSION',16,296,w-32,63,14,'left');caps(string.format('%d.%d',avg//10,avg%10),16,296,w-32,63,30,'right');caps('A GOOD TABLE',16,364,w-32,29,14,'left');caps('2.5',16,364,w-32,29,14,'right')
 else for _,row in ipairs({{262,'ONLY THE PRACTICE ROUND SO FAR,'},{296,'AND IT DOES NOT SCORE.'},{340,'PLAY ONE THAT DOES AND THE'},{369,'AVERAGE APPEARS HERE.'}})do caps(row[2],16,row[1],w-32,29,14,'left')end end
 caps('HOW FAR OFF, ALL TIME',16,404,w-32,29,14,'left');caps('ROUNDS',16,404,w-32,29,14,'right');ornament(16,434,w-32,168);button('PLAY ROUND '..(g.round+1),box(16,612,w-32,58),'start');caps('ENDING CLEARS THIS SCORE.',16,676,w-32,29,14,'left');caps('RECORD SINCE UPDATE IS KEPT.',16,705,w-32,29,14,'left');button('END THE SESSION',box(16,736,w-32,54),'fresh',nil,true)
end
function V.init(g)
 V.sessionStarted=g.phase>0 or g.round>0;F.init();art.look=assert(smudge.read_file('art/look.raw',1800));art.pass92=assert(smudge.read_file('art/pass92.raw',1104));local ok,orientation=pcall(smudge.get_orientation);V.originalOrientation=ok and orientation or nil;pcall(smudge.set_orientation,'portrait');previous=nil
end
function V.exit()if V.peek then smudge.full_refresh()end;V.peek=false;V.touchPeek=false;V.buttonHeld=false;if V.originalOrientation then pcall(smudge.set_orientation,V.originalOrientation)end end
function V.draw(g,M,notice)
 if previous~=g.phase then V.focus=1;V.peek=false;V.touchPeek=false;V.buttonHeld=false;V.everRevealed=false;V.nudge=false;if g.phase~=3 then V.clue=false end;previous=g.phase end
 local w,h=smudge.get_bounds();V.hit={};smudge.clear();local view
 if V.screen=='front'or g.phase==0 and V.screen=='menu'and not V.pendingPass then view='menu';menu(g,w,h)
 elseif V.screen=='help'then view='help';help(w,h)elseif V.screen=='pause'then view='pause';pause(g,w,h)elseif V.screen=='summary'then view='summary';summary(g,w,h)
 elseif V.pendingPass or g.phase==6 then view='pass';pass(g,w,h)elseif g.phase==1 then view='pick';pick(g,M,w,h)elseif g.phase==2 then view='peek';peek(g,M,w,h)elseif g.phase==3 and V.clue then view='clue';clue(g,M,w,h)elseif g.phase==3 then view='dial';dial(g,M,w,h)elseif g.phase==4 then view='call';call(g,M,w,h)else view='reveal';reveal(g,M,w,h)end
 V.currentView=view;V.g=g;V.paintedPhase=g.phase;V.paintedScreen=V.screen;V.paintedPass=V.pendingPass;V.paintedClue=V.clue
 if V.keyboard and V.hit[V.focus]then outline(V.hit[V.focus].box,3)end
 if notice~=''then F.block(notice,box(16,h-34,w-32,30),14,1)end
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
