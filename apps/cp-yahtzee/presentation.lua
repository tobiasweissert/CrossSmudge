local Q=dofile('front.lua')
local F=dofile('graphics.lua');local B=dofile('board.lua');local V={page=1};local state,boxes,L
function V.init()F.init()end
local help={'ROLL FIVE DICE, UP TO THREE TIMES PER TURN.','TAP DICE TO KEEP THEM. THEN ROLL AGAIN.','TAKE ONE EMPTY CATEGORY TO END YOUR TURN.','AN UPPER TOTAL OF 63 EARNS 35 BONUS POINTS.','AN EXTRA YAHTZEE AFTER A SCORED 50 EARNS 100.','A JOKER MUST TAKE ITS MATCHING UPPER BOX FIRST.','THEN A LOWER BOX; THEN ANOTHER UPPER BOX.','TWO PEOPLE MAY SHARE THIS DEVICE.'}
function V.draw(Y,g,mode,screen,selection,menu,notice,search,items)
 local w,h=smudge.get_bounds();if screen=='menu'and state~='menu'then Q.home()end;state=screen;boxes={};smudge.clear();F.header('YAHTZEE')
 if screen=='board' then L=B.draw(Y,g,mode,selection,search,F,w,h)
 elseif screen=='menu' then Q.hasSave=V.saved;Q.draw(Y,g,mode,F,w,h,items,B.preview,'CURRENT GAME  '..Y.total(g.cards[1])..' - '..Y.total(g.cards[2]))
 elseif screen=='new' or screen=='take' then
  local title=screen=='new' and 'REPLACE THE CURRENT GAME?' or 'TAKE '..Y.names[selection-6]..' FOR '..Y.score_for(g.cards[g.turn],g.d,selection-6)..'?'
  local y=130;for _,line in ipairs(F.wrap(title,w-48,20))do F.text(line,24,y,20);y=y+34 end
  boxes={{x=16,y=h-140,w=w-32,h=52},{x=16,y=h-76,w=w-32,h=52}}
  F.pill('CANCEL',boxes[1],menu==1);F.pill('CONFIRM',boxes[2],menu==2)
 else
  local lesson=B.lesson(V.page,F,w,h);V.page=lesson.page;V.pages=lesson.pages;boxes=lesson.boxes
 end
 if notice~='' then F.center(F.fit(notice,w-32,10),16,h-105,w-32,22,10)end
end
function V.button(screen,button)
 if screen=='menu'then return Q.button(button)end
 if screen~='help' then return false end
 if (button=='right' or button=='down' or button=='confirm')and V.page>=4 then return 'done' end
 if button=='left' or button=='up' then V.page=math.max(1,V.page-1)
 elseif button=='right' or button=='down' or button=='confirm' then V.page=math.min(4,V.page+1)else return false end
 return true
end
function V.touch(x,y)
 x,y=math.floor(x),math.floor(y)
 local w,h=smudge.get_bounds();if x<0 or y<0 or x>=w or y>=h then return end
 if y<76 then if state=='menu'then local handled=Q.button('back');if handled then return 'refresh'end end;return 'back' end
 if state=='menu'then return Q.touch(x,y)end
 if state=='board' then return B.hit(L,x,y,F)end
 for n,b in ipairs(boxes)do if F.inside(b,x,y)then
  if state=='help' then if V.page>=4 then return 'back' end;V.page=V.page+1;return 'refresh' end
  return state=='menu' and 'menu' or 'confirm',n
 end end
end
return V
