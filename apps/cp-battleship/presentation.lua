local Q=dofile('front.lua')
local G=dofile('graphics.lua');local B=dofile('board.lua')
local V={helpPage=1};local boxes,L,state
function V.init()G.init();if Q.init then Q.init()end end
function V.persist(M,g)if Q.observe then Q.observe(g)end;return not Q.persist or Q.persist()end
function V.begin()if Q.begin then Q.begin()end end
function V.records()return Q.records and Q.records()end
V.nativePlacement=B.place~=nil
function V.layout()return L end
function V.focusTarget(g,index)
 if not V.nativePlacement then return end
 B.notice=nil
 B.aim=g.phase==1 and g.status==0 and g.turn==1 and g.show==0 and g.sb:sub(index,index)=='0'and index or 0
end
function V.confirmTarget(g,index)
 if B.aim>0 then return true end
 V.focusTarget(g,index)
 return false
end
function V.place(M,g,at)return B.place and B.place(M,g,at)or false end
function V.played(index)if B.record then B.record(index)end;if V.nativePlacement then B.notice=nil;B.aim=0 end end
function V.draw(M,g,mode,screen,cursor,menu,notice,search,items,focus)
 if Q.observe then Q.observe(g)end
 local w,h=smudge.get_bounds();if screen=='menu'and state~='menu'then Q.home()end;state=screen;boxes={};smudge.clear();G.header(M.title)
 if screen=='board' then
  L=B.draw(M,g,mode,cursor,search,G,w,h,focus)
  if notice~='' then G.center(G.fit(notice,w-32,10),16,h-105,w-32,22,10)end
 elseif screen=='menu' then
  Q.hasSave=V.saved and (not V.nativePlacement or g.status==0);Q.draw(M,g,mode,G,w,h,items,B.preview,string.upper(M.message(g)))
 elseif screen=='nearby' then
  G.header('NEARBY');G.center('HOST SUPPORT REQUIRED',16,128,w-32,52,20)
  local y=216;for _,line in ipairs(G.wrap('NEARBY PLAY IS NOT EXPOSED BY THIS LUA HOST.',w-48,20))do G.text(line,24,y,20);y=y+38 end
  boxes={{x=16,y=h-68,w=(w-44)//2,h=52},{x=(w+12)//2,y=h-68,w=(w-44)//2,h=52}}
  G.status('HOW TO PLAY',boxes[1]);G.pill('BACK',boxes[2],false)
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
 if screen=='nearby'and(button=='back'or button=='confirm')then return'done'end
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
 if state=='nearby'then
  if G.inside(boxes[1],x,y)then return'help'elseif G.inside(boxes[2],x,y)then return'back'end
  return
 end
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
