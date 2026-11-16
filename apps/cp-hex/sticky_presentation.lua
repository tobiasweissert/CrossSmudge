local Q=dofile('sticky_front.lua')
local G=dofile('sticky_graphics.lua');local B=dofile('sticky_board.lua')
local P=dofile('sticky_preferences.lua')
local V={helpPage=1,settingsRow=1,preferences=P};local boxes,L,state
function V.init()G.init();Q.init()end
function V.load(M,g,mode)P.load(M,g,mode);B.last=P.lastMove>0 and P.lastMove or nil end
function V.begin(g)P.begin(g);B.last=nil end
function V.persist(M,g,mode)P.observe(M,g,mode);return P.save(M,g)end
function V.layout()return L end
function V.played(index)P.lastMove=index;if B.record then B.record(index)end end
function V.draw(M,g,mode,screen,cursor,menu,notice,search,items)
 P.observe(M,g,mode);Q.wins,Q.losses=P.wins,P.losses
 local w,h=smudge.get_bounds();if screen=='menu'and state~='menu'then Q.home()end;state=screen;boxes={};smudge.clear();G.header(M.title)
 if screen=='board' then
  L=B.draw(M,g,mode,cursor,search,G,w,h,P.seat)
  if notice~='' then G.center(G.fit(notice,w-32,10),16,h-105,w-32,22,10)end
 elseif screen=='menu' then
  Q.hasSave=V.saved and P.pending==0;local picture=g;if P.pending==1 then picture=P.last~=''and M.decode(P.last)or M.new()end;Q.draw(M,picture,mode,G,w,h,items)
 elseif screen=='settings'then
  G.header('SETTINGS');local labels={'OPPONENT','LEVEL','YOU PLAY'}
  local values={P.nextMode==1 and 'COMPUTER'or'2 PLAYERS',P.nextMode==1 and 'UNAVAILABLE'or'--',P.nextMode==1 and(P.nextSeat==1 and'BLACK'or'WHITE')or'--'}
  for i,label in ipairs(labels)do local b={x=16,y=119+(i-1)*66,w=w-32,h=62};boxes[i]=b;local chosen=i==V.settingsRow and(P.nextMode==1 or i==1)
   smudge.rect(b.x,b.y,b.w,b.h,true,chosen);if not chosen then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)end
   G.text(label,b.x+12,b.y+19,20,chosen);G.text(values[i],b.x+b.w-12-G.inkWidth(values[i],20),b.y+19,20,chosen)
  end
 elseif screen=='nearby'or screen=='level'then
  G.header(screen=='nearby'and'NEARBY'or'LEVEL');G.nativeText(screen=='nearby'and'NEARBY PLAY IS NOT EXPOSED BY THIS LUA HOST.'or'ORIGINAL STRENGTH LEVELS ARE UNAVAILABLE. THE EXISTING PORT OPPONENT IS FIXED.',16,119,w-32,h<600 and 112 or 200,h<600 and 10 or 20)
  boxes={{x=16,y=h-68,w=w-32,h=52}};G.status('BACK',boxes[1])
 elseif screen=='confirm' then
  G.center('REPLACE THE CURRENT BOARD?',16,130,w-32,42,20)
  boxes={{x=16,y=h-140,w=w-32,h=52},{x=16,y=h-76,w=w-32,h=52}}
  G.pill('CANCEL',boxes[1],menu==1);G.pill('START NEW GAME',boxes[2],menu==2)
 elseif screen=='help' and B.lesson then
  local lesson=B.lesson(V.helpPage,G,w,h);V.helpPage=lesson.page;V.helpPages=lesson.pages;V.singleHelp=true;V.wholeHelp=lesson.whole;boxes=lesson.boxes
 else
  V.singleHelp=false;V.wholeHelp=false
  local help=B.help or M.help
  local pages=math.max(1,(#help+1)//2);V.helpPage=math.max(1,math.min(pages,V.helpPage));V.helpPages=pages;G.header('HOW TO PLAY',V.helpPage..' OF '..pages)
  local y=120;local size=h<600 and 10 or 20
  for n=(V.helpPage-1)*2+1,math.min(#help,V.helpPage*2) do
   for _,line in ipairs(G.wrap(help[n],w-48,size))do G.text(line,24,y,size);y=y+(size==20 and 34 or 22)end;y=y+22
  end
  G.center(V.helpPage..' OF '..pages,16,h-150,w-32,32,20)
  boxes={{x=16,y=h-80,w=(w-44)//2,h=52},{x=(w+12)//2,y=h-80,w=(w-44)//2,h=52}}
  G.pill('PREV',boxes[1],false);G.pill('NEXT',boxes[2],false)
 end
end
function V.button(screen,button)
 if screen=='menu'then return Q.button(button)end
 if screen=='nearby'or screen=='level'then if button=='confirm'or button=='back'then return'done'end;return false end
 if screen=='settings'then
  if button=='confirm'then return'setting',V.settingsRow end
  if button=='up'or button=='left'then V.settingsRow=(V.settingsRow-2)%3+1;return true
  elseif button=='down'or button=='right'then V.settingsRow=V.settingsRow%3+1;return true end
  return false
 end
 if screen~='help' then return false end
 if (button=='confirm' or button=='down' or button=='right')and V.helpPage>=(V.helpPages or 1)then return 'done' end
 if button=='up' or button=='left' then V.helpPage=math.max(1,V.helpPage-1)
 elseif button=='down' or button=='right' or button=='confirm' then V.helpPage=V.helpPage+1 else return false end
 return true
end
function V.touch(x,y)
 x,y=math.floor(x),math.floor(y)
 local w,h=smudge.get_bounds();if x<0 or y<0 or x>=w or y>=h then return end
 if y<76 then if state=='menu'then local handled=Q.button('back');if handled then return 'refresh'end end;return 'back' end
 if state=='menu'then return Q.touch(x,y)end
 if state=='settings'then for i,b in ipairs(boxes)do if G.inside(b,x,y)then V.settingsRow=i;return'setting',i end end;return end
 if state=='nearby'or state=='level'then if G.inside(boxes[1],x,y)then return'back'end;return end
 if state=='board' then return B.hit(L,x,y,G) end
 if state=='help' and V.wholeHelp and y>=119 then if V.helpPage>=V.helpPages then return 'back' end;V.helpPage=V.helpPage+1;return 'refresh' end
 for i,b in ipairs(boxes or {})do if G.inside(b,x,y)then
  if state=='help' then
   if V.singleHelp then if V.helpPage>=V.helpPages then return 'back' end;V.helpPage=V.helpPage+1
   else V.helpPage=math.max(1,V.helpPage+(i==1 and -1 or 1))end;return 'refresh' end
  return state=='menu' and 'menu' or 'confirm',i
 end end
end
return V
