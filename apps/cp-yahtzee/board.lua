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
 value=tostring(value);F.text(value,right-F.width(value,size),b.y+(b.h-(size==20 and 25 or 13))//2,size)
end
function B.draw(Y,g,mode,selection,search,F,w,h)
 local l=B.layout(w,h);local human=mode==2 or g.turn==1;local ours=mode==2 and g.turn or 1
 if Y.over(g)then
  local first,second=Y.total(g.cards[1]),Y.total(g.cards[2]);F.header(first==second and 'A TIE' or mode==2 and (first>second and 'P1 WINS' or 'P2 WINS')or first>second and 'YOU WIN' or 'THEY WIN')
  F.center(first..' - '..second,16,h<600 and 112 or 127,w-32,h<600 and 50 or 70,30);F.text('BIGGEST SWINGS',16,h<600 and 172 or 217,10)
  local used={};local y=h<600 and 206 or 257
  for shown=1,3 do local best,gap=0,0
   for c=1,13 do local difference=math.abs(g.cards[1][c]-g.cards[2][c]);if not used[c]and difference>gap then best,gap=c,difference end end
   if best==0 then break end;used[best]=true;F.text(names[best],16,y,10)
   local label=g.cards[1][best]..' - '..g.cards[2][best];F.text(label,w-16-F.width(label,10),y,10);y=y+(h<600 and 28 or 36)
  end
  local a,b=Y.upper(g.cards[1])>=63 and 35 or 0,Y.upper(g.cards[2])>=63 and 35 or 0
  if a~=b then F.text('TOP HALF BONUS  '..a..' - '..b,16,y,10)end
  l.again={x=16,y=h-132,w=w-32,h=52};l.done={x=16,y=h-68,w=w-32,h=52};F.pill('PLAY AGAIN',l.again,true);F.pill('DONE',l.done,true);return l
 end
 for i,b in ipairs(l.dice)do
  if g.rolls>0 then
   smudge.rect(b.x,b.y,b.w,b.h,true,false);smudge.rect(b.x,b.y,b.w,b.h,false,3,true);F.die(b.x,b.y,b.w,g.d[i],false,9)
   if g.held&(1<<(i-1))~=0 then smudge.rect(b.x-8,b.y-8,b.w+16,b.h+16,false,4,true)end
  end
 end
 if g.rolls==0 then F.center(human and 'ROLL TO START YOUR TURN' or 'THEY ARE ROLLING',16,104,w-32,40,20)end
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
  smudge.rect(b.x,b.y,b.w,kind=='total' and 3 or 1,true,true);F.text(kind=='bonus' and 'BONUS' or 'TOTAL',b.x,b.y+(b.h-13)//2,10)
  for column=1,2 do local card=g.cards[column==1 and ours or 3-ours];local right=column==1 and b.your or b.their;local text
   if kind=='total' then text=Y.total(card)else
    local upper=Y.upper(card);local maximum=upper
    for c=1,6 do if card[c]==-1 then maximum=maximum+5*c end end
    text=upper>=63 and '+35' or maximum>=63 and (63-upper)..' MORE' or 'NO BONUS'
   end
   number(F,text,right-8,b,10)
  end
 end
 local label=Y.over(g) and 'GAME OVER' or not human and 'THEIR TURN' or g.rolls>=3 and 'TAKE A BOX' or g.rolls==0 and 'ROLL' or 'ROLL AGAIN ('..(3-g.rolls)..' LEFT)'
 F.pill(label,l.roll,human and g.rolls<3 and not Y.over(g))
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 for n,b in ipairs(l.dice)do if x>=b.x-8 and x<b.x+b.w+8 and y>=b.y-8 and y<b.y+b.h+8 then return 'select',n end end
 if F.inside(l.roll,x,y)then return 'select',6 end
 for n,b in ipairs(l.rows)do if F.inside(b,x,y)then return 'select',n+6 end end
end
B.lesson=dofile('lesson.lua')
function B.preview(Y,g,F,area)
 local y=area.y+(area.h-180)//2;F.center('CURRENT SCORE',area.x,y,area.w,24,10);F.center(Y.total(g.cards[1])..' - '..Y.total(g.cards[2]),area.x,y+28,area.w,64,30)
 if g.rolls>0 then local size=math.min(62,(area.w-48)//5);local x=area.x+(area.w-5*size-48)//2;for i=1,5 do local xx=x+(i-1)*(size+12);smudge.rect(xx,y+110,size,size,false,3,true);F.die(xx,y+110,size,g.d[i],false,math.max(3,size*9//62))end end
end
return B
