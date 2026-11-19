local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_front.lua')end
local nearby
return function(M,g,F,w,h,labels,selected,preview,status)
 F.header('TOY BATTLE');F.native(status or'NO BATTLES YET',{x=16,y=119,w=w-32,h=26},10,'left');smudge.rect(16,151,w-32,3,true,true)
 local count=#labels;local top=h-16-(count*62+(count-1)*6+12);local area={x=16,y=154,w=w-32,h=top-154-16};preview(M,g,F,area)
 local boxes={};nearby=nearby or assert(smudge.read_file('art/nearby32.raw',128))
 for i,label in ipairs(labels)do local b={x=16,y=top+(i-1)*66,w=w-32,h=62};boxes[i]=b;smudge.rect(b.x,b.y,b.w,b.h,i==selected,1,true);F.native(label,{x=b.x+12,y=b.y,w=b.w-24,h=b.h},20,'left',i==selected)
  if label=='CONTINUE'then F.native(g.medals[1]..'-'..g.medals[2],{x=b.x+12,y=b.y,w=b.w-24,h=b.h},20,'right',i==selected)end
  if label=='PLAY NEARBY'then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,nearby,i==selected)end
 end
 return boxes
end
