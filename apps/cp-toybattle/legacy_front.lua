-- Source front-door band order: actual record/position, rule, ornament, doors.
return function(M,g,F,w,h,labels,selected,preview,status)
 F.header(M.title=='Go - local'and 'GO'or'TOY BATTLE');F.text(F.fit(status,w-32,10),16,99,10);smudge.rect(16,128,w-32,3,true,true)
 local width=w>h and(w-48)//2 or w-32;local left=w>h and w-width-16 or 16;local top=h-16-#labels*62-(#labels-1)*4;local boxes={}
 local area={x=16,y=145,w=w>h and left-32 or w-32,h=w>h and h-175 or top-166};preview(M,g,F,area)
 for i,label in ipairs(labels)do local b={x=left,y=top+(i-1)*66,w=width,h=62};boxes[i]=b;smudge.rect(b.x,b.y,b.w,b.h,i==selected,1,true);F.text(F.fit(label,b.w-60,20),b.x+12,b.y+18,20,i==selected);F.center(i==1 and '>'or i==2 and '+'or'?',b.x+b.w-48,b.y+12,36,38,20,i==selected)end
 return boxes
end
