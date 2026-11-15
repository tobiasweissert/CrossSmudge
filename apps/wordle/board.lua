local U=dofile('design.lua');local V={}
local rows={{1,2,3,4,5,6,7,8,9,10},{11,12,13,14,15,16,17,18,19},{28,21,22,23,24,25,26,27,20}}
local labels={'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K','L','Del','Z','X','C','V','B','N','M','Enter','Clear','Menu'}
function V.init()U.init()end
function V.layout(w,h)
 local landscape=w>h;local gap=6
 local tile=landscape and math.min(44,(h-160-5*gap)//6) or math.min(64,(w-48-4*gap)//5,(h-416)//6)
 local side=tile*5+4*gap
 local l={w=w,h=h,tile=tile,y=landscape and 88 or 102,x=landscape and 24 or (w-side)//2,footer=h,keys={}}
 local kx=landscape and side+60 or 4;local kwidth=w-kx-(landscape and 16 or 4)
 local kh=landscape and 54 or math.min(61,(h-590-16)//3);local ky=landscape and 148 or h-3*(kh+6)-4
 l.ky=ky;l.kh=kh;l.kw=(kwidth-9*6)//10
 for row,ids in ipairs(rows) do
  local unit=(kwidth-9*6)//10;local special=math.floor(unit*1.65)
  local width=row==2 and unit*9+8*6 or row==3 and special*2+unit*7+8*6 or unit*10+9*6
  local x=kx+(kwidth-width)//2
  for _,id in ipairs(ids) do
   local wid=(id==28 or id==20) and special or unit
   l.keys[id]={x=x,y=ky+(row-1)*(kh+6),w=wid,h=kh,label=labels[id]};x=x+wid+6
  end
 end
 l.legendY=landscape and h-35 or l.y+6*(tile+gap)+18
 return l
end
function V.touch(l,x,y,hold,action,menu)
 if not hold and y<64 then menu();return end
 for id,b in pairs(l.keys) do
  if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then action(hold and id==20 and 29 or id);return end
 end
end
function V.draw(l,g,notice,state,action)
 on_long_press=function(x,y)if not state.closed and not state.menu then V.touch(l,x,y,true,action) end end
 smudge.clear();U.header('Five letters','Guess '..math.min(6,g and #g.rows+1 or 1)..' of 6',l.w)
 for row=1,6 do for col=1,5 do
  local x,y=l.x+(col-1)*(l.tile+6),l.y+(row-1)*(l.tile+6)
  local mark=g and g.marks[row] and g.marks[row][col] or 0
  local word=g and (g.rows[row] or row==#g.rows+1 and g.typing) or ''
  local letter=word and word:sub(col,col) or ''
  local tone=mark==4 and 'black' or mark==3 and 'gray' or 'white'
  U.tile(x,y,l.tile,l.tile,tone,(mark>=3 or letter~='') and 'black' or 'gray')
  if letter~='' then U.text(letter,x+l.tile//2,y+l.tile//2,l.tile>=48 and 'b36' or 'b20','center',mark==4,mark==2) end
 end end
 local legend={{'black','right spot'},{'gray','wrong spot'},{'white','not in word'}}
 local lx=l.w>l.h and 24 or 54;local group=l.w>l.h and 132 or 130
 for i,pair in ipairs(legend) do local x=lx+(i-1)*group;U.tile(x,l.legendY-9,18,18,pair[1],pair[1]=='white' and 'gray' or pair[1]);U.text(pair[2],x+26,l.legendY,'r20',nil,false,true) end
 if l.w<l.h then smudge.line(0,l.ky-8,l.w-1,l.ky-8,true) end
 for id,b in pairs(l.keys) do
  local mark=g and g.keys[labels[id]] or 0;local tone=(id==28 or mark==4) and 'black' or mark==3 and 'gray' or 'white'
  if mark==2 then U.dash(b.x,b.y,b.w,b.h) else U.tile(b.x,b.y,b.w,b.h,tone,'black') end
  if id==20 then
   local cx,cy=b.x+b.w//2,b.y+b.h//2
   smudge.line(cx-13,cy,cx-6,cy-9,true);smudge.line(cx-6,cy-9,cx+12,cy-9,true);smudge.line(cx+12,cy-9,cx+12,cy+9,true);smudge.line(cx+12,cy+9,cx-6,cy+9,true);smudge.line(cx-6,cy+9,cx-13,cy,true)
   smudge.line(cx-2,cy-4,cx+6,cy+4,true);smudge.line(cx-2,cy+4,cx+6,cy-4,true)
  else U.text(b.label,b.x+b.w//2,b.y+b.h//2,'b20','center',tone=='black',mark==2) end
 end
 if notice~='' and not notice:find('Legacy practice',1,true) then
  notice=U.wrap(notice,l.w-56,'r20',1)[1] or ''
  local width=math.min(l.w-32,math.max(180,U.width(notice,'r20')+24));local x=(l.w-width)//2
  U.round(x,68,width,28,6,true,true);U.text(notice,l.w//2,82,'r20','center')
 end
end
function V.drawMenu(l,state)
 local labels={'Resume','Today','Previous date','Next date','New practice','Daily statistics','Exit'}
 local w,h=math.min(380,l.w-48),64+#labels*44+12
 local x,y=(l.w-w)//2,(l.h-h)//2
 U.round(x,y,w,h,12,true,true);U.round(x,y,w,h,12,false,false)
 U.text('Game menu',x+24,y+30,'b28')
 l.menu={};l.mx=x+16;l.my=y+64;l.mw=w-32
 for n,label in ipairs(labels) do
  local b={x=x+16,y=y+64+(n-1)*44,w=w-32,h=40};l.menu[n]=b
  U.round(b.x,b.y,b.w,b.h,6,state.menuIndex==n,false)
  U.text(label,b.x+b.w//2,b.y+b.h//2,'b20','center',state.menuIndex==n)
 end
end
return V
