-- Original YahtzeeScreens.cpp dice, two score columns and fifteen-line card.
local B={};local names={'ONES','TWOS','THREES','FOURS','FIVES','SIXES','THREE OF A KIND','FOUR OF A KIND','FULL HOUSE','SMALL STRAIGHT','LARGE STRAIGHT','YAHTZEE','CHANCE'}
function B.layout(w,h)
 local l={w=w,h=h,rows={},dice={},roll={x=16,y=h-68,w=w-32,h=52}}
 local size,gap=62,12;local left=(w-size*5-gap*4)//2
 for i=1,5 do l.dice[i]={x=left+(i-1)*(size+gap),y=95,w=size,h=size}end
 local landscape=w>h;local rh=landscape and math.min(30,(h-260)//8)or 36
 local width=landscape and (w-48)//2 or w-32
 for c=1,13 do
  local group=landscape and (c>6 and 1 or 0)or 0
  local line=landscape and (c>6 and c-7 or c-1)or (c>6 and c or c-1)
  local x=16+group*(width+16)
  l.rows[c]={x=x,y=180+line*rh,w=width,h=rh,your=x+width-128,their=x+width}
 end
 l.bonus={x=16,y=180+6*rh,w=width,h=rh,your=16+width-128,their=16+width}
 l.total={x=16,y=180+(landscape and 7 or 14)*rh,w=width,h=rh,your=16+width-128,their=16+width}
 return l
end
local function number(F,value,right,b,size)
 F.nativeText(tostring(value),right-100,b.y,100,b.h,size,'right',false,size==20)
end
function B.draw(Y,g,mode,selection,search,F,w,h)
 local l=B.layout(w,h);local human=mode==2 or g.turn==1;local ours=mode==2 and g.turn or 1
 if Y.over(g)then
  local first,second=Y.total(g.cards[1]),Y.total(g.cards[2]);F.header(first==second and 'A TIE' or mode==2 and (first>second and 'P1 WINS' or 'P2 WINS')or first>second and 'YOU WIN' or 'THEY WIN')
  local a,b=Y.upper(g.cards[1])>=63 and 35 or 0,Y.upper(g.cards[2])>=63 and 35 or 0;local gaps=0;for c=1,13 do if g.cards[1][c]~=g.cards[2][c]then gaps=gaps+1 end end
  local compact=257+(math.min(3,gaps)+(a~=b and 1 or 0))*36>h-132;local rowHeight=compact and 28 or 36
  F.nativeText(first..' - '..second,16,compact and 107 or 127,w-32,compact and 60 or 70,30);F.nativeText('BIGGEST SWINGS',16,compact and 179 or 217,w-32,rowHeight,10,'left')
  local used={};local y=compact and 211 or 257
  for shown=1,3 do local best,gap=0,0
   for c=1,13 do local difference=math.abs(g.cards[1][c]-g.cards[2][c]);if not used[c]and difference>gap then best,gap=c,difference end end
   if best==0 then break end;used[best]=true;F.nativeText(names[best],16,y,260,rowHeight,10,'left')
   local label=g.cards[1][best]..' - '..g.cards[2][best];F.nativeText(label,w-156,y,140,rowHeight,10,'right');y=y+rowHeight
  end
  if a~=b then F.nativeText('TOP HALF BONUS  '..a..' - '..b,16,y,w-32,rowHeight,10,'left')end
  l.again={x=16,y=h-132,w=w-32,h=52};l.done={x=16,y=h-68,w=w-32,h=52};F.status('PLAY AGAIN',l.again);F.status('DONE',l.done);return l
 end
 for i,b in ipairs(l.dice)do
  if g.rolls>0 then
   smudge.rect(b.x,b.y,b.w,b.h,true,false);smudge.rect(b.x,b.y,b.w,b.h,false,3,true);F.die(b.x,b.y,b.w,g.d[i],false,9)
   if g.held&(1<<(i-1))~=0 then smudge.rect(b.x-8,b.y-8,b.w+16,b.h+16,false,4,true)end
  end
 end
 if g.rolls==0 then F.nativeText(human and'ROLL TO START YOUR TURN'or'THEY ARE ROLLING',16,114,w-32,26,20,nil,false,true)end
 local groups=w>h and 2 or 1
 for group=0,groups-1 do local b=l.rows[group==0 and 1 or 7]
  number(F,mode==2 and 'P'..ours or 'YOU',b.your,{y=158,h=22},10);number(F,mode==2 and 'P'..(3-ours)or 'THEM',b.their,{y=158,h=22},10)
 end
 for c,b in ipairs(l.rows)do
  F.text(names[c],b.x,b.y+(b.h-13)//2,10)
  for column=1,2 do local card=g.cards[column==1 and ours or 3-ours];local right=column==1 and b.your or b.their
   if card[c]~=-1 then number(F,card[c],right-8,b,10)
   else
    smudge.rect(right-62,b.y+4,62,(b.h>=32 and b.h-12 or math.max(19,b.h-8)),false,3,true)
    if column==1 and human and g.rolls>0 and Y.allowed(card,g.d,c)then number(F,Y.score_for(card,g.d,c),right-8,b,10)end
   end
  end
  if selection==c+6 then smudge.rect(b.x-2,b.y,b.w+4,b.h,false,2,true)end
 end
 for _,kind in ipairs({'bonus','total'})do local b=l[kind]
  smudge.rect(b.x,b.y,b.w,kind=='total' and 3 or 1,true,true);if kind=='bonus'then F.nativeText('BONUS',b.x,b.y+4,b.w,b.h,10,'left')else F.nativeText('TOTAL',b.x,b.y+6,b.w,b.h,20,'left',false,true)end
  for column=1,2 do local card=g.cards[column==1 and ours or 3-ours];local right=column==1 and b.your or b.their;local text
   if kind=='total' then text=Y.total(card)else
    local upper=Y.upper(card);local maximum=upper
    for c=1,6 do if card[c]==-1 then maximum=maximum+5*c end end
    text=upper>=63 and '+35' or maximum>=63 and (63-upper)..' MORE' or 'GONE'
   end
   number(F,text,right-8,{y=b.y+(kind=='total'and 6 or 4),h=b.h},kind=='total'and 20 or 10)
  end
 end
 if g.cards[1][14]+g.cards[2][14]>0 then local row=l.rows[12];F.nativeText('+'..((g.cards[1][14]+g.cards[2][14])*100),row.x+150,row.y,110,row.h,10,'left')end
 local label=Y.over(g) and 'GAME OVER' or not human and 'THEIR TURN' or g.rolls>=3 and 'TAKE A BOX' or g.rolls==0 and 'ROLL' or 'ROLL AGAIN ('..(3-g.rolls)..' LEFT)'
 local joker=g.rolls>0 and human and g.cards[ours][12]==50 and g.cards[ours][g.d[1]]==-1 and g.d[1]==g.d[2]and g.d[1]==g.d[3]and g.d[1]==g.d[4]and g.d[1]==g.d[5]
 if joker then label=g.rolls>=3 and'YAHTZEE! TAKE THE MARKED BOX'or'YAHTZEE! ONE BOX ONLY'end
 F.status(label,l.roll,not human or g.rolls>=3)
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 for n,b in ipairs(l.dice)do if x>=b.x-8 and x<b.x+b.w+8 and y>=b.y-8 and y<b.y+b.h+8 then return 'select',n end end
 if F.inside(l.roll,x,y)then return 'select',6 end
 for n,b in ipairs(l.rows)do if F.inside(b,x,y)then return 'select',n+6 end end
end
-- Original YahtzeeScreens.cpp held-hand examples and lower-section pay table.
local lines={
 'ROLL FIVE DICE. TAP THE ONES YOU WANT TO KEEP AND ROLL AGAIN, UP TO THREE ROLLS.',
 'THEN PUT THE HAND IN ONE BOX ON YOUR CARD. EVERY BOX IS USED EXACTLY ONCE.',
 'SIXTY-THREE IN THE TOP HALF EARNS THIRTY-FIVE MORE. THIRTEEN TURNS EACH.',
 'A SECOND YAHTZEE SCORES 100 MORE, AND MAY FORCE THE ONE MARKED BOX.'}
local faces={{5,5,2,5,3},{5,5,5,5,1},{4,4,4,4,4}};local held={11,15,31}
local names={'THREE OF A KIND','FOUR OF A KIND','FULL HOUSE','SMALL STRAIGHT','LARGE STRAIGHT','YAHTZEE','CHANCE'}
local values={'ALL FIVE DICE','ALL FIVE DICE','25','30','40','50','ALL FIVE DICE'}
local function lesson(page,F,w,h)
 page=math.max(1,math.min(4,page));F.header('HOW TO PLAY',page..' OF 4')
 F.nativeText(lines[page],16,h<600 and 107 or 119,w-32,h<600 and 100 or 200,h<600 and 10 or 20)
 if page<4 then
  local size=62;local x=(w-5*size-4*12)//2;local y=h<600 and 260 or 493
  for i=1,5 do local xx=x+(i-1)*(size+12);smudge.rect(xx,y,size,size,false,3,true);F.die(xx,y,size,faces[page][i],false,9)
   if held[page]&(1<<(i-1))~=0 then smudge.rect(xx-8,y-8,size+16,size+16,false,4,true)end
  end
 else
  local top=h<600 and 207 or 412;local row=h<600 and 26 or 32
  for i,name in ipairs(names)do local y=top+(i-1)*row;F.text(name,16,y+(row-13)//2,10);F.text(values[i],w-16-F.inkWidth(values[i],10),y+(row-13)//2,10)
   if i>1 then smudge.rect_dither(16,y-2,w-32,1,true)end
  end
 end
 local next={x=16,y=h-68,w=w-32,h=52};F.status(page==4 and'GOT IT'or'NEXT',next)
 return {page=page,pages=4,boxes={next}}
end

B.lesson=lesson
function B.recordPreview(R,F,w,h)
 local hand=R.yahtzees>0 and R.face>0
 if w>h then
  F.nativeText('PERSONAL BEST',16,154,220,24,10);F.nativeText(tostring(R.best),16,182,220,60,30)
  if hand then local size=28;local x=w-220;for i=1,5 do local xx=x+(i-1)*36;smudge.rect(xx,176,size,size,false,2,true);F.die(xx,176,size,R.face,false,4)end;F.nativeText(R.yahtzees..' YAHTZEES ROLLED',w-240,212,224,24,10)end
  return
 end
 local block=88+(hand and 118 or 0);local top=154+((h-226-154)>block and(h-226-154-block)//2 or 12)
 F.nativeText('PERSONAL BEST',16,top,w-32,24,10);F.nativeText(tostring(R.best),16,top+28,w-32,64,30)
 if hand then local x=(w-5*62-4*12)//2;local y=top+110
  for i=1,5 do local xx=x+(i-1)*74;smudge.rect(xx,y,62,62,true,false);smudge.rect(xx,y,62,62,false,3,true);F.die(xx,y,62,R.face,false,9)end
  F.nativeText(R.yahtzees..' YAHTZEES ROLLED',16,y+76,w-32,24,10)
 end
end
function B.preview(Y,g,F,area)
 local y=area.y+(area.h-180)//2;F.center('CURRENT SCORE',area.x,y,area.w,24,10);F.center(Y.total(g.cards[1])..' - '..Y.total(g.cards[2]),area.x,y+28,area.w,64,30)
 if g.rolls>0 then local size=math.min(62,(area.w-48)//5);local x=area.x+(area.w-5*size-48)//2;for i=1,5 do local xx=x+(i-1)*(size+12);smudge.rect(xx,y+110,size,size,false,3,true);F.die(xx,y+110,size,g.d[i],false,math.max(3,size*9//62))end end
end
return B
