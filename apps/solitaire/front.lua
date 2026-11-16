-- Original CrossPlay SolitaireScreens::buildMenu; fixed landscape composition,
-- stacked record ornament in portrait. Uses the shipped original Jersey cuts.
local V={};local F
function V.init(graphics)F=graphics end
local function button(label,b,outline)
 smudge.rect(b.x,b.y,b.w,b.h,true,not outline)
 if outline then smudge.rect(b.x,b.y,b.w,b.h,false,true,1)end
 F.buttonText(label,b,not outline)
end
function V.draw(w,h,hasSave,moves,three,records)
 smudge.clear();smudge.rect(0,0,w,56,true,true);smudge.rect(0,60,w,3,true,true);F.text('SOLITAIRE',16,9,30,true)
 local top=96
 F.text(hasSave and 'DEAL IN PLAY'or'FRESH DECK',16,top+9,30)
 F.text(hasSave and moves..' MOVES DEEP'or'SHUFFLED AND WAITING',16,top+60,20)
 local played=records and records.played or 0
 F.text(played==0 and 'NEVER PLAYED. NO SHAME IN THAT.'or played..' DEALT, '..records.wins..' CLEARED, '..records.streak..' IN A ROW',16,top+105,10)
 local handX=w>h and 456 or 16;local handY=w>h and top+10 or top+154
 for i=0,15 do
  local x,y=handX+i%8*26,handY+i//8*92;local result=records and records.recent[i+1]or 0
  smudge.rounded_rect(x,y,52,74,5,true,false)
  if result==1 then F.roundDither(x+4,y+4,44,66,3,true)end
  smudge.rounded_rect(x,y,52,74,5,false,true,result==0 and 1 or 2)
  if result==2 then records.pip(x+7,y+10,i%4)end
 end
 F.text(played==0 and 'SIXTEEN DEALS TO FILL'or'YOUR LAST SIXTEEN',handX,handY+199,10)
 local count=hasSave and 3 or 2;local gap=12;local bw=(w-32-gap*(count-1))//count
 local menu={};local labels=hasSave and {'PICK IT UP','DEAL FRESH',three and 'TURNING THREE'or'TURNING ONE'}or{'DEAL',three and 'TURNING THREE'or'TURNING ONE'}
 for i=1,count do local b={x=16+(i-1)*(bw+gap),y=h-68,w=bw,h=52};menu[i]=b;button(labels[i],b,i>1)end
 return{w=w,h=h,menu=menu,hasSave=hasSave,drawThree=three}
end
function V.win(w,h,moves,records)
 smudge.clear();smudge.rect(0,0,w,56,true,true);smudge.rect(0,60,w,3,true,true);F.text('SOLITAIRE',16,9,30,true)
 local step=(w-94)//27
 for i=0,27 do
  local lift=(i-14)*(i-14)*30//196
  local x,y=18+i*step,h-16-86-lift
  smudge.rounded_rect(x,y,58,86,6,true,false);smudge.rounded_rect(x,y,58,86,6,false,true,1)
  if i%3==0 then records.pip(x+4,y+26,(i//3)%4)end
 end
 local top=90
 F.center(F.fit('ALL FIFTY-TWO HOME',w,30),0,top,w,64,30)
 local brags={'%d MOVES. THE DECK NEVER STOOD A CHANCE.','%d MOVES, AND NOT ONE OF THEM WASTED.','%d MOVES. SOMEBODY HAS DONE THIS BEFORE.','%d MOVES. THE STOCK IS STILL RECOVERING.'}
 local brag=F.fit(string.format(brags[moves%4+1],moves),w,20);F.text(brag,(w-F.inkWidth(brag,20))//2,top+70,20)
 local stats=records.wins..' CLEARED, '..records.streak..' IN A ROW';F.text(stats,(w-F.inkWidth(stats,10))//2,top+109,10)
 local bw=w>h and 210 or (w-44)//2
 local buttons={{x=w//2-bw-6,y=top+144,w=bw,h=52},{x=w//2+6,y=top+144,w=bw,h=52}}
 button('DEAL AGAIN',buttons[1],false);button('BACK TO THE TABLE',buttons[2],true)
 return {w=w,h=h,won=buttons}
end

return V
