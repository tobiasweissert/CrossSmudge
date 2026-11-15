-- CrossPlay Sudoku screen geometry and Jersey masks; rules remain in app.lua.
local F=dofile('graphics.lua')
local V={level=0,focus=1,help=0,drawer=false}
local levels={'EASY','MEDIUM','HARD','EXPERT'}
local lessons={
 {'THE RULE','EVERY ROW, COLUMN AND BOX','HOLDS 1 TO 9. NO DIGIT TWICE.'},
 {'WRITING','PICK A DIGIT, THEN TAP A CELL.','TAP A CLUE TO PICK ITS DIGIT UP.'},
 {'CLEARING','TAP YOUR OWN DIGIT AGAIN','TO CLEAR IT.'},
 {'PENCIL','HOLD A CELL TO PENCIL A DIGIT.','HOLD AGAIN TO RUB IT OUT.'},
 {'MISTAKES','A CLASH TURNS BLACK.','HINT NAMES A CELL YOU CAN SOLVE.'}}
local function frame(x,y,w,h,n,paper)smudge.rect(x,y,w,h,false,n,not paper)end
local function button(label,b,selected,size)
 smudge.rect(b.x,b.y,b.w,b.h,selected,selected and true or 1,true)
 F.center(label,b.x,b.y,b.w,b.h,size or 20,selected)
end
local function inside(b,x,y)return b and x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h end
local function corners(x,y,w,h,arm,n,paper)
 for _,dx in ipairs({0,w-arm}) do for _,dy in ipairs({0,h-n}) do smudge.rect(x+dx,y+dy,arm,n,true,not paper) end end
 for _,dx in ipairs({0,w-n}) do for _,dy in ipairs({0,h-arm}) do smudge.rect(x+dx,y+dy,n,arm,true,not paper) end end
end
local function header(w,right)
 smudge.rect(0,0,w,76,true,true);F.text('SUDOKU',16,19,30,true)
 F.text(right,w-16-F.width(right,20),26,20,true);smudge.rect(0,80,w,3,true,true)
end
function V.init(g)F.init();V.level=g.level end
function V.layout(w,h)
 local landscape=w>h
 local size=landscape and math.min(50,(h-129)//9) or math.min(50,(w-30)//9)
 local outer=9*size+18;local left=landscape and 6 or (w-outer)//2
 local l={w=w,h=h,x=left+9,y=104,size=size,outer=outer,left=left,footer=h,buttons={},keys={},menu={}}
 local key=landscape and 56 or 69;local px=landscape and w-key*3-16 or left
 local py=landscape and 95 or h-16-key*3
 for d=1,9 do l.keys[d]={x=px+(d-1)%3*key,y=py+(d-1)//3*key,w=key,h=key} end
 local rx=landscape and left+outer+16 or left+key*3+14
 local ry=landscape and py+key*3+16 or py
 local rw=landscape and w-rx-16 or left+outer-rx
 local rh=landscape and (h-ry-16-12)//3 or 61
 local gap=landscape and 6 or (key*3-rh*3)//2
 l.status={x=rx,y=ry,w=rw,h=rh}
 l.buttons[3]={x=rx,y=ry+rh+gap,w=rw,h=rh}
 l.buttons[2]={x=rx,y=ry+2*(rh+gap),w=rw,h=rh}
 l.buttons[4]={x=0,y=0,w=w,h=76}
 l.play={x=16,y=h-212,w=w-32,h=52}
 l.difficulty={x=16,y=h-148,w=w-32,h=62}
 l.howto={x=16,y=h-80,w=w-32,h=62}
 return l
end
local function miniature(l,g)
 local side=math.min(l.w-56,math.max(90,l.play.y-218));local cell=side//9;side=cell*9
 local x,y=(l.w-side)//2,190+(l.play.y-202-side)//2
 corners(x-12,y-12,side+24,side+24,16,3)
 for i=0,9,3 do smudge.rect_dither(x+i*cell,y,1,side,true);smudge.rect_dither(x,y+i*cell,side,1,true) end
 if not g.given then return end
 for i=1,81 do local n=g.given[i]~=0 and 2 or g.value[i]~=0 and cell//3
  if n then smudge.rect(x+(i-1)%9*cell+n,y+(i-1)//9*cell+n,cell-2*n,cell-2*n,true,true) end
 end
end
local function drawMenu(l,g)
 header(l.w,levels[V.level+1]);local empty=81
 if g.value then for _,value in ipairs(g.value) do if value~=0 then empty=empty-1 end end end
 F.text(g.solved and 'LAST ONE SOLVED' or g.given and empty..' LEFT' or 'NOT STARTED',16,130,20)
 if g.notice~='' then local notice=string.upper(g.notice);while F.width(notice,10)>l.w-32 do notice=notice:sub(1,-2) end;F.text(notice,16,166,10) end
 miniature(l,g)
 local resume=g.given and not g.solved and V.level==g.level
 button(resume and 'RESUME' or 'NEW PUZZLE',l.play,V.focus==1)
 button('DIFFICULTY: '..levels[V.level+1],l.difficulty,V.focus==2)
 button('HOW TO PLAY',l.howto,V.focus==3)
 if V.drawer then
  local x,y=l.w//2-150,l.h//2-140
  smudge.rect(x-8,y-8,316,288,true,false);frame(x-8,y-8,316,288,3)
  for n,name in ipairs(levels) do local b={x=x,y=y+(n-1)*68,w=300,h=62};l.menu[n]=b;button(name,b,V.level==n-1) end
 end
end
local function related(i,j)return (i-1)//9==(j-1)//9 or (i-1)%9==(j-1)%9 or (i-1)//27==(j-1)//27 and ((i-1)%9)//3==((j-1)%9)//3 end
local function drawBoard(l,g)
 header(l.w,levels[g.level+1]);smudge.rect(l.left,95,l.outer,l.outer,true,true);smudge.rect(l.x,l.y,l.size*9,l.size*9,true,false)
 local empty=81
 for i=1,81 do local x,y=l.x+(i-1)%9*l.size,l.y+(i-1)//9*l.size;local value=g.value and g.value[i] or 0
  local clash=false;local taken=0
  if g.value then for j=1,81 do if j~=i and related(i,j) and g.value[j]~=0 then
   taken=taken|(1<<(g.value[j]-1));if value~=0 and g.value[j]==value then clash=true end
  end end end
  if clash then smudge.rect(x,y,l.size,l.size,true,true)
  elseif g.given and g.given[i]~=0 then smudge.rect_dither(x,y,l.size,l.size,false) end
  if value~=0 then
   empty=empty-1;F.center(tostring(value),x,y,l.size,l.size,l.size>=42 and 30 or 20,clash)
   if value==g.digit then corners(x,y,l.size,l.size,math.min(13,l.size//4),3,clash) end
  elseif g.notes then for d=1,9 do if g.notes[i]&(1<<(d-1))&~taken~=0 then
   F.center(tostring(d),x+4+(d-1)%3*((l.size-8)//3),y+4+(d-1)//3*((l.size-8)//3),(l.size-8)//3,(l.size-8)//3,10)
  end end end
  if i==g.cell and g.notice~='' then frame(x,y,l.size,l.size,4) end
 end
 for i=1,8 do local weight=i%3==0 and 5 or 1;local offset=i*l.size-weight//2
  smudge.rect(l.x+offset,l.y,weight,l.size*9,true,true);smudge.rect(l.x,l.y+offset,l.size*9,weight,true,true)
 end
 for d,b in ipairs(l.keys) do
  local count=0;if g.value then for _,value in ipairs(g.value) do if value==d then count=count+1 end end end
  if count>=9 then smudge.rect_dither(b.x,b.y,b.w,b.h,false) end
  frame(b.x,b.y,b.w,b.h,d==g.digit and 4 or 1);F.center(tostring(d),b.x,b.y,b.w,b.h,30)
 end
 local label=g.job and 'MAKING ONE' or g.solved and 'SOLVED' or g.notice~='' and string.upper(g.notice) or empty..' LEFT'
 local font=F.width(label,20)>l.status.w-12 and 10 or 20
 while F.width(label,font)>l.status.w-12 and #label>0 do label=label:sub(1,-2) end
 button(label,l.status,g.solved,font);button('UNDO',l.buttons[3],false);button('HINT',l.buttons[2],false)
end
function V.draw(l,g)
 smudge.clear()
 if V.help>0 then
  header(l.w,V.help..' OF 5');local lesson=lessons[V.help]
  F.center(lesson[1],16,140,l.w-32,50,30)
  for i=2,3 do F.center(lesson[i],16,210+(i-2)*38,l.w-32,34,20) end
  button(V.help==5 and 'GOT IT' or 'NEXT',{x=16,y=l.h-68,w=l.w-32,h=52},true)
 elseif g.menu then drawMenu(l,g) else drawBoard(l,g) end
end
local function play(g,menuSelect)
 g.menuIndex=g.given and not g.solved and V.level==g.level and 1 or V.level+2;menuSelect()
end
function V.button(g,b,menuSelect,refresh)
 if V.help>0 then
  if b=='back' then V.help=0 elseif b=='confirm' or b=='down' or b=='right' then V.help=V.help==5 and 0 or V.help+1 end
 elseif g.menu then
  if V.drawer then
   if b=='back' or b=='confirm' then V.drawer=false elseif b=='up' or b=='left' then V.level=(V.level-1)%4 else V.level=(V.level+1)%4 end
  elseif b=='confirm' then if V.focus==1 then play(g,menuSelect) elseif V.focus==2 then V.drawer=true else V.help=1 end
  elseif b=='up' or b=='left' then V.focus=(V.focus-2)%3+1 elseif b=='down' or b=='right' then V.focus=V.focus%3+1 else return false end
 else return false end
 refresh();return true
end
function V.touch(l,g,x,y,event,enter,action,menuSelect,refresh)
 if V.help>0 then if event=='tap' then if y<83 then V.help=0 elseif y>=l.h-68 then V.help=V.help==5 and 0 or V.help+1 end;refresh() end;return end
 if g.menu then
  if event~='tap' then return end
  if V.drawer then for n,b in ipairs(l.menu) do if inside(b,x,y) then V.level=n-1;V.drawer=false end end
  elseif inside(l.play,x,y) then play(g,menuSelect)
  elseif inside(l.difficulty,x,y) then V.drawer=true
  elseif inside(l.howto,x,y) then V.help=1 end
  refresh();return
 end
 if event~='tap' and event~='hold' then return end
 local c,r=(x-l.x)//l.size,(y-l.y)//l.size
 if c>=0 and c<9 and r>=0 and r<9 then g.cell=r*9+c+1;enter(g.cell,event=='hold' or g.notesMode);return end
 if event~='tap' then return end
 for digit,b in ipairs(l.keys) do if inside(b,x,y) then g.digit=digit;refresh();return end end
 for id,b in pairs(l.buttons) do if inside(b,x,y) then action(id);return end end
end
return V
