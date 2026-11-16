-- Sticky-only controller. The rules and core save journal remain unchanged.
local M=dofile('core.lua');local S=dofile('storage.lua');local F=dofile('sticky_graphics.lua');local B=dofile('sticky_board.lua');local Front=dofile('sticky_front.lua');local P=dofile('sticky_preferences.lua')
local g,aim,L;local screen='menu';local cursor=1;local hasSave=false;local notice='';local boxes={};local icons={}
local function redraw()smudge.request_update()end
local function save()
 P.observe(M,g);if not P.save(M,g)then notice='UI SAVE FAILED';return false end
 if not S.save(M.encode(g))then notice='SAVE FAILED';return false end
 P.commit();hasSave=true;notice='';return true
end
local function changed()aim=nil;redraw()end
local function fresh()
 if not save()then redraw();return end
 g=M.new(smudge.millis(),P.nextBoard,P.nextMode==1 and P.nextHandicap or 0);g.solo=P.nextMode==1 and 1 or 0;P.begin(g);screen='board';changed()
end
local function setting(row)
 if row==1 then P.nextMode=3-P.nextMode
 elseif row==2 and P.nextMode==1 then screen='level';notice='';redraw();return
 elseif row==3 and P.nextMode==1 then P.nextHandicap=P.nextHandicap==0 and 2 or P.nextHandicap==5 and 0 or P.nextHandicap+1
 elseif row==4 and P.nextMode==1 and P.nextHandicap==0 then P.nextSeat=3-P.nextSeat
 elseif row==5 then P.nextBoard=P.nextBoard==9 and 13 or P.nextBoard==13 and 19 or 9
 else return end
 P.pending=1;notice='';redraw()
end
local function activate()
 if screen=='settings'then setting(cursor)
 elseif screen=='nearby'then screen='menu';cursor=1;redraw()
 elseif screen=='level'then screen='settings';cursor=2;redraw()
 elseif cursor==1 then if P.pending==1 or g.phase==2 then fresh()else screen='board';redraw()end
 elseif cursor==2 then screen='nearby';redraw()
 else screen='settings';cursor=1;redraw()end
end
local function myTurn()return g.solo==0 or g.turn==P.seat end
local function play(p)
 if g.phase==1 then if M.mark(g,p)then changed()end
 elseif g.phase==0 and myTurn()and M.trial(g,p)then
  if aim==p then if M.play(g,p)then changed()end else aim=p;redraw()end
 end
end
local function action(other)
 if g.phase==0 then if myTurn()and M.play(g,0)then changed()end
 elseif g.phase==1 then
  if other then g.phase=0;g.passes=0;g.dead=string.rep('0',#g.b);g.accepted=0
  else -- Manual approval: this port has no automatic dead-stone judgement.
   M.accept(g,1);M.accept(g,2)
  end;changed()
 elseif other then if save()then screen='menu';cursor=1;aim=nil;redraw()end
 else fresh()end
end
function on_init()
 F.init();for _,name in ipairs({'computer','humans','level','handicap','colour','board'})do icons[name]=assert(smudge.read_file('art/native-'..name..'32.raw',128))end
 g=S.load(M.decode);hasSave=g~=nil;if not g then g=M.new(smudge.millis());g.solo=1 end;M.pause(g);P.load(M,g);screen='menu';redraw()
end
function on_draw()
 local w,h=smudge.get_bounds();smudge.clear();boxes={};P.observe(M,g)
 if screen=='menu'then
  local progress=hasSave and P.pending==0 and g.phase<2
  boxes=Front(M,g,F,w,h,{progress and'RESUME GAME'or'PLAY','PLAY NEARBY','SETTINGS'},cursor,B.preview,P)
 elseif screen=='settings'then
  F.header('SETTINGS');local shared=P.nextMode==2
  local labels={'OPPONENT','LEVEL','HANDICAP','YOU PLAY','BOARD'};local values={shared and'2 PLAYERS'or'COMPUTER',shared and'--'or'UNAVAILABLE',shared and'--'or P.nextHandicap==0 and'NONE'or P.nextHandicap..' STONES',shared and'--'or P.nextHandicap>0 and'BLACK'or P.nextSeat==1 and'BLACK'or'WHITE',P.nextBoard..'x'..P.nextBoard}
  local names={shared and'humans'or'computer','level','handicap','colour','board'}
  local height=62;local pitch=66;local top=119
  for i,label in ipairs(labels)do local enabled=i==1 or i==5 or not shared and(i~=4 or P.nextHandicap==0);local active=i==cursor and enabled;local b={x=16,y=top+(i-1)*pitch,w=w-32,h=height};boxes[i]=b
   smudge.rect(b.x,b.y,b.w,b.h,true,active);if not active then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)end
   smudge.bitmap(b.x+12,b.y+(b.h-32)//2,32,32,icons[names[i]],active);F.nativeText(label,b.x+54,b.y,b.w-150,b.h,20,'left',active);F.nativeText(values[i],b.x,b.y,b.w-12,b.h,20,'right',active)
  end
  if not shared and h>=600 then F.nativeText('FIXED ONE-PLY HEURISTIC. STRENGTH LEVELS AND AUTOMATIC DEAD-STONE REVIEW ARE UNAVAILABLE.',16,h<600 and 391 or 489,w-32,h<600 and 75 or 90,10,'left')end
 elseif screen=='nearby'or screen=='level'then F.header(screen=='level'and'LEVEL'or'NEARBY');F.nativeText(screen=='level'and'FIXED ONE-PLY HEURISTIC. ORIGINAL STRENGTH LEVELS AND AUTOMATIC DEAD-STONE REVIEW ARE UNAVAILABLE.'or'NEARBY PLAY IS NOT EXPOSED BY THIS LUA HOST.',16,119,w-32,180,h<600 and 10 or 20);boxes[1]={x=16,y=h-68,w=w-32,h=52};F.status('BACK',boxes[1])
 elseif screen=='discard'then F.header('GO');F.nativeText('REPLACE THE SAVED GAME?',16,119,w-32,120,20);boxes={{x=16,y=h-132,w=w-32,h=52},{x=16,y=h-68,w=w-32,h=52}};F.status('CANCEL',boxes[1]);F.status('NEW GAME',boxes[2])
 else L=B.draw(M,g,aim,F,w,h,P.seat)end
 if notice~=''then F.nativeText(notice,16,h-106,w-32,26,10)end
end
function on_button(b,p)
 if not p then return end
 if screen=='board'then
  if b=='back'then if save()then screen='menu';cursor=1;aim=nil end
  elseif b=='confirm'then if g.phase~=0 then action(false)elseif aim then play(aim)else action(false)end
  elseif g.phase==0 and myTurn()then local p=aim or(g.n*g.n//2+1);local delta=(b=='up'or b=='page_back')and-g.n or(b=='down'or b=='page_forward')and g.n or b=='left'and-1 or 1;aim=(p-1+delta)%(g.n*g.n)+1 end
 elseif screen=='discard'then if b=='back'then screen='menu'elseif b=='confirm'then fresh()end
 elseif b=='back'then if screen=='menu'then if save()then smudge.exit()end elseif screen=='level'then screen='settings';cursor=2 else screen='menu';cursor=1 end
 elseif b=='confirm'then activate()
 elseif screen=='settings'or screen=='menu'then local count=screen=='settings'and 5 or 3;cursor=(cursor+((b=='up'or b=='left'or b=='page_back')and-2 or 0))%count+1 end;redraw()
end
function on_touch(x,y,e)
 if e~='tap'then return end;x,y=math.floor(x),math.floor(y);local w,h=smudge.get_bounds();if x<0 or y<0 or x>=w or y>=h then return end
 if y<76 then on_button('back',true);return end
 if screen=='board'then if F.inside(L.pass,x,y)then action(false)elseif L.other and F.inside(L.other,x,y)then action(true)else local p=B.hit(L,g.n,x,y);if p then play(p)end end
 elseif screen=='discard'then if F.inside(boxes[1],x,y)then screen='menu';redraw()elseif F.inside(boxes[2],x,y)then fresh()end
 else for i,b in ipairs(boxes)do if F.inside(b,x,y)then if screen=='menu'and i==1 and hasSave and P.pending==0 and g.phase<2 and x>=b.x+b.w-62 then screen='discard';redraw();return end;cursor=i;activate();return end end end
end
function on_update()if screen=='board'and g.phase==0 and g.solo==1 and not myTurn()then M.play(g,M.ai(g));changed()end end
function on_swipe(d)on_button(d,true)end
function on_exit()M.pause(g);save()end
return {state=function()return g end,layout=function()return L end,preferences=P,aim=function()return aim end,screen=function()return screen end}
