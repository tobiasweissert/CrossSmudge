local Q={selected=1,wins=0,losses=0};local boxes={};local nearby;local B=dofile('sticky_board.lua')
function Q.init()nearby=assert(smudge.read_file('art/nearby32.raw',128))end
function Q.home()Q.selected=1 end
function Q.draw(M,g,mode,F,w,h,items)
 B.menu(g,F,w,h,Q.wins,Q.losses);boxes={};local top=h-226
 local labels={Q.hasSave and g.status==0 and 'RESUME GAME'or'PLAY','PLAY NEARBY','SETTINGS'}
 for i,label in ipairs(labels)do local b={x=16,y=top+(i-1)*66,w=w-32,h=62};boxes[i]=b;local active=i==Q.selected
  smudge.rect(b.x,b.y,b.w,b.h,true,active);if not active then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)end
  F.text(label,b.x+12,b.y+19,20,active);if i==2 then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,nearby,active)end
 end
end
local function activate()if Q.selected==2 then return'nearby'elseif Q.selected==3 then return'settings'end;return'activate',1 end
function Q.button(button)
 if button=='confirm'then return activate()end
 if button=='up'or button=='left'or button=='page_back'then Q.selected=(Q.selected-2)%3+1
 elseif button=='down'or button=='right'or button=='page_forward'then Q.selected=Q.selected%3+1 else return false end;return true
end
function Q.touch(x,y)for i,b in ipairs(boxes)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then Q.selected=i;local kind,id=activate();return kind=='activate'and'menu'or kind,id end end end
return Q
