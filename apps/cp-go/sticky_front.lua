local icons={}
local function draw(M,g,F,w,h,labels,selected,preview,status)
 if not icons.play then for _,name in ipairs({'play','settings','trash'})do icons[name]=assert(smudge.read_file('art/native-'..name..'32.raw',128))end;icons.nearby=assert(smudge.read_file('art/nearby32.raw',128))end
 F.header('GO');F.text(status.wins+status.losses==0 and'NO GAMES YET'or (status.wins+status.losses)..' PLAYED   '..status.wins..' WON',16,125,10);smudge.rect(16,151,w-32,3,true,true)
 local top=h-226;local boxes={};local inProgress=labels[1]=='RESUME GAME'
 local snapshot=inProgress and g or status.history(M)
 if snapshot then preview(M,snapshot,F,{x=16,y=154,w=w-32,h=top-154},inProgress,status)end
 for i,label in ipairs(labels)do local b={x=16,y=top+(i-1)*66,w=w-32,h=62};boxes[i]=b;local active=i==selected
  smudge.rect(b.x,b.y,b.w,b.h,true,active);if not active then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)end;F.text(label,b.x+12,b.y+19,20,active)
  if i==1 and inProgress then
   smudge.rect(b.x+b.w-62,b.y,62,62,true,false);smudge.rect(b.x+b.w-62,b.y,62,62,false,3,true);smudge.bitmap(b.x+b.w-47,b.y+15,32,32,icons.trash,false)
  else smudge.bitmap(b.x+b.w-56,b.y+15,32,32,i==1 and icons.play or i==2 and icons.nearby or icons.settings,active)end
 end
 return boxes
end
return draw
