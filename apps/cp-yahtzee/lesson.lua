-- Original YahtzeeScreens.cpp held-hand examples and lower-section pay table.
local lines={
 'ROLL FIVE DICE. TAP THE ONES YOU WANT TO KEEP AND ROLL AGAIN, UP TO THREE ROLLS.',
 'THEN PUT THE HAND IN ONE BOX ON YOUR CARD. EVERY BOX IS USED EXACTLY ONCE.',
 'SIXTY-THREE IN THE TOP HALF EARNS THIRTY-FIVE MORE. THIRTEEN TURNS EACH.',
 'A SECOND YAHTZEE SCORES 100 MORE, AND MAY FORCE THE ONE MARKED BOX.'}
local faces={{5,5,2,5,3},{5,5,5,5,1},{4,4,4,4,4}};local held={11,15,31}
local names={'THREE OF A KIND','FOUR OF A KIND','FULL HOUSE','SMALL STRAIGHT','LARGE STRAIGHT','YAHTZEE','CHANCE'}
local values={'ALL FIVE DICE','ALL FIVE DICE','25','30','40','50','ALL FIVE DICE'}
return function(page,F,w,h)
 page=math.max(1,math.min(4,page));F.header('HOW TO PLAY',page..' OF 4')
 F.lessonText(lines[page],16,119,w-32,h<600 and 10 or 20)
 if page<4 then
  local size=62;local x=(w-5*size-4*12)//2;local y=h<600 and 260 or 493
  for i=1,5 do local xx=x+(i-1)*(size+12);smudge.rect(xx,y,size,size,false,3,true);F.die(xx,y,size,faces[page][i],false,9)
   if held[page]&(1<<(i-1))~=0 then smudge.rect(xx-8,y-8,size+16,size+16,false,4,true)end
  end
 else
  local top=h<600 and 207 or 412;local row=h<600 and 26 or 32
  for i,name in ipairs(names)do local y=top+(i-1)*row;F.text(name,16,y+(row-13)//2,10);F.text(values[i],w-16-F.width(values[i],10),y+(row-13)//2,10)
   if i>1 then smudge.rect_dither(16,y-2,w-32,1,true)end
  end
 end
 local next={x=16,y=h-68,w=w-32,h=52};F.pill(page==4 and 'GOT IT' or 'NEXT',next,true)
 return {page=page,pages=4,boxes={next}}
end
