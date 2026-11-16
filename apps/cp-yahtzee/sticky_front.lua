local Q={selected=1};local boxes,nearby={},nil
function Q.init()nearby=assert(smudge.read_file('art/nearby32.raw',128))end
function Q.home()Q.selected=1 end
function Q.draw(M,g,mode,F,w,h,items,preview)
 F.text(Q.records.played..' PLAYED   '..Q.records.won..' WON',16,125,10);smudge.rect(16,151,w-32,3,true,true)
 if Q.records.played>0 then preview(Q.records,F,w,h)end
 local labels={'PLAY','PLAY NEARBY','HOW TO PLAY'};boxes={};local top=h-226
 for i,label in ipairs(labels)do local b={x=16,y=top+(i-1)*66,w=w-32,h=62};boxes[i]=b;local selected=i==Q.selected
  smudge.rect(b.x,b.y,b.w,b.h,true,selected);if not selected then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)end
  F.text(label,b.x+12,b.y+19,20,selected)
  if i==2 then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,nearby,selected)end
 end
end
local function activate()if Q.selected==2 then return 'nearby'end;return 'activate',Q.selected==1 and 1 or 4 end
function Q.button(button)
 if button=='confirm'then return activate()end
 if button=='up'or button=='left'or button=='page_back'then Q.selected=(Q.selected-2)%3+1
 elseif button=='down'or button=='right'or button=='page_forward'then Q.selected=Q.selected%3+1
 else return false end;return true
end
function Q.touch(x,y)
 for i,b in ipairs(boxes)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then Q.selected=i;local kind,id=activate();return kind=='activate'and'menu'or kind,id end end
end
return Q
