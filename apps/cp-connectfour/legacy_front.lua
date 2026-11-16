-- Original bottom-anchored front-door bands; only actions backed by this port.
local Q={page='home',selected=1};local rows,boxes={},{ }
function Q.home()Q.page='home';Q.selected=1 end
function Q.draw(M,g,mode,F,w,h,items,preview,status)
 local setup=Q.page=='settings';local battle=#items==6
 rows=setup and (battle and {{'NEW GAME',4},{'ROTATE PLACEMENT',2},{'OWN / TARGET GRID',3},{'SAVE AND EXIT',6},{'BACK',0}}or{{'NEW SOLO GAME',2},{'NEW TWO-PLAYER GAME',3},{'SAVE AND EXIT',5},{'BACK',0}})or{{Q.hasSave and 'RESUME GAME' or 'PLAY',1},{'SETTINGS',0},{'HOW TO PLAY',#items-1}}
 Q.selected=math.min(Q.selected,#rows);boxes={};local landscape=w>h;local width=landscape and (w-48)//2 or w-32;local left=landscape and w-width-16 or 16;local row=62;local top=h-16-#rows*row-(#rows-1)*4
 if setup then F.header('SETTINGS');top=106 else
  F.text(F.fit(status or 'CURRENT GAME',w-32,10),16,99,10);smudge.rect(16,128,w-32,3,true,true)
  local area={x=16,y=145,w=landscape and left-32 or w-32,h=landscape and h-175 or top-166}
  if preview then preview(M,g,F,area)end
 end
 for i,rowData in ipairs(rows)do local b={x=left,y=top+(i-1)*(row+4),w=width,h=row};boxes[i]=b;smudge.rect(b.x,b.y,b.w,b.h,i==Q.selected,1,true);F.text(F.fit(rowData[1],b.w-64,20),b.x+12,b.y+(b.h-25)//2,20,i==Q.selected)
  if not setup then local cx,cy=b.x+b.w-29,b.y+b.h//2;local ink=i~=Q.selected
   if i==1 then for n=0,16 do smudge.rect(cx-7+n,cy-10+n//2,1,21-n,true,ink)end
   elseif i==2 then smudge.circle(cx,cy,11,false,ink);smudge.circle(cx,cy,4,true,ink);for n=-1,1,2 do smudge.rect(cx+n*15-2,cy-3,5,6,true,ink);smudge.rect(cx-3,cy+n*15-2,6,5,true,ink)end
   else F.center('?',cx-16,cy-20,32,40,20,i==Q.selected)end
  end
 end
end
local function select()
 local id=rows[Q.selected][2]
 if id==0 then Q.page=Q.page=='home'and 'settings'or'home';Q.selected=1;return true end
 return 'activate',id
end
function Q.button(button)
 if #rows==0 then return button=='confirm'and 'activate'or false,1 end
 if button=='back'then if Q.page=='settings'then Q.home();return true end;return false end
 if button=='confirm'then return select()end
 if button=='up'or button=='left'or button=='page_back'then Q.selected=(Q.selected-2)%#rows+1
 elseif button=='down'or button=='right'or button=='page_forward'then Q.selected=Q.selected%#rows+1 else return false end;return true
end
function Q.touch(x,y)
 for i,b in ipairs(boxes)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then Q.selected=i;local kind,id=select();return kind=='activate'and 'menu'or 'refresh',id end end
end
return Q
