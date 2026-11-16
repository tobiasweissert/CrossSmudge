local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
if not ok or device~='sticky'then return dofile('legacy_board.lua')end
-- Original ConnectFourScreens.cpp slab, lip and remaining-disc trays.
local B={}
function B.layout(w,h)
 local cell=math.min(64,(w-32)//7,(h-260)//6)
 local lip=math.min(42,cell);local x=(w-cell*7)//2;local y=116
 if w>h then x=24;cell=math.min(48,(h-160)//6);lip=32;y=106 end
 return {x=x,y=y,cell=cell,lip=lip,w=w,h=h}
end
local function winning(g)
 if g.status==0 or g.status==3 then return end
 for row=0,5 do for col=0,6 do for _,d in ipairs({{1,0},{0,1},{1,1},{1,-1}})do
  local ex,ey=col+3*d[1],row+3*d[2]
  if ex>=0 and ex<7 and ey>=0 and ey<6 then
   local ok=true;for step=0,3 do if g.b[(row+step*d[2])*7+col+step*d[1]+1]~=g.status then ok=false end end
   if ok then return col,row,d[1],d[2] end
  end
 end end end
end
function B.draw(M,g,mode,cursor,search,F,w,h)
 local l=B.layout(w,h);local side=l.cell*7;local gh=l.cell*6;local frame=9
 if g.status>0 then
  F.header(g.status==3 and 'A DRAW' or mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WIN' or 'THEY WIN')
  if w>h then l.cell=math.min(l.cell,(h-257)//6);side=l.cell*7;gh=l.cell*6;l.x=(w-side)//2 end
  l.lip=0;l.y=95+math.max(0,(h-144-95-gh-18)//2)+9
  l.again={x=16,y=h-132,w=w-32,h=52};l.done={x=16,y=h-68,w=w-32,h=52}
 end
 smudge.rect(l.x-frame,l.y-frame,side+2*frame,gh+l.lip+2*frame,true,true)
 smudge.rect_dither(l.x,l.y,side,gh+l.lip,false)
 local live=g.status==0 and (mode==2 or g.turn==1)
 if l.lip>0 then for col=0,6 do
  local cx=l.x+col*l.cell+l.cell//2;local cy=l.y+l.lip//2
  if live and g.b[35+col+1]==0 then F.ring(cx,cy,math.min(16,l.cell//3),g.turn==2,4)
  else F.ditherDisc(cx,cy,math.min(16,l.cell//3),true)end
 end
 end
 if l.lip>0 then smudge.rect(l.x,l.y+l.lip-3,side,3,true,true)end
 local col,row,dx,dy=winning(g)
 for r=0,5 do for c=0,6 do
  local cx=l.x+c*l.cell+l.cell//2;local cy=l.y+l.lip+r*l.cell+l.cell//2
  local v=g.b[(5-r)*7+c+1];local radius=math.min(26,l.cell//2-5)
  F.disc(cx,cy,radius,false)
  if v~=0 then F.ring(cx,cy,radius,v==2,4)end

 end end
 -- Original buildResult passes a null line on a draw, retaining its last-disc dot.
 if (g.status==0 or g.status==3)and #g.history>0 then
  local c=tonumber(g.history:sub(-1));local r=0
  while r<6 and g.b[r*7+c]~=0 do r=r+1 end
  local v=g.b[(r-1)*7+c];F.disc(l.x+(c-1)*l.cell+l.cell//2,l.y+l.lip+(6-r)*l.cell+l.cell//2,8,v==1)
 end
 if col then
  local x0=l.x+col*l.cell+l.cell//2;local y0=l.y+(5-row)*l.cell+l.cell//2
  local steps=3*l.cell;for n=0,steps do smudge.rect(x0+dx*n-3,y0-dy*n-3,7,7,true,g.status~=2)end
 end
 if l.again then F.status('PLAY AGAIN',l.again);F.status('DONE',l.done);return l end
 local counts={21,21};for _,v in ipairs(g.b)do if v~=0 then counts[v]=counts[v]-1 end end
 local tx,ty=w>h and side+60 or l.x,w>h and 210 or h-146
 for strip=0,1 do local seat=strip==0 and 2 or 1;local cy=ty+strip*28
  F.text(tostring(counts[seat]),tx,cy-1,10)
  local pitch=w>h and 10 or 18
  for n=1,counts[seat]do F.ring(tx+44+8+(n-1)*pitch,cy+6,w>h and 4 or 8,seat==2,2)end
 end
 local text=g.status==3 and 'DRAW' or g.status>0 and (mode==2 and 'P'..g.status..' WINS' or g.status==1 and 'YOU WON' or 'THEY WON') or mode==2 and (g.turn==1 and 'LIGHT DROP' or 'DARK DROP') or live and 'YOUR DROP' or 'THEIR DROP'
 F.status(text,{x=w>h and side+60 or 16,y=h-68,w=w>h and w-side-84 or w-32,h=52},true)
 -- Native lip itself is the drop target; no cursor frame obscures its discs.
 return l
end
function B.hit(l,x,y,F)
 if l.again then if F.inside(l.again,x,y)then return 'new' elseif F.inside(l.done,x,y)then return 'back' end;return end
 if x>=l.x and x<l.x+l.cell*7 and y>=l.y and y<l.y+l.lip+l.cell*6 then return 'cell',(x-l.x)//l.cell+1 end
end
-- Original ConnectFourScreens.cpp three teaching fragments.
local lines={
 'TAP A COLUMN. YOUR DISC FALLS TO THE LOWEST FREE PLACE IN IT.',
 'FOUR IN A LINE WINS. ACROSS, UP, OR ON EITHER DIAGONAL LIKE THIS ONE.',
 'A FULL COLUMN STOPS TAKING DISCS, AND ITS SLOT AT THE TOP GOES GREY.'}
local boards={ {'....','....','.L..','.D..'}, {'...L','..LD','.LDD','LDLD'}, {'D...','L...','D...','LD..'} }
local function lesson(page,F,w,h)
 page=math.max(1,math.min(3,page));F.header('HOW TO PLAY',page..' OF 3')
 F.lessonText(lines[page],16,119,w-32,h<600 and 10 or 20)
 local cell=h<600 and 40 or 64;local side=cell*4;local x=(w-side)//2;local y=h<600 and 218 or 389;local radius=math.floor(cell*26/64)
 smudge.rect(x-9,y-9,side+18,side+18,true,true);smudge.rect_dither(x,y,side,side,false)
 for row=0,3 do for col=0,3 do local cx,cy=x+col*cell+cell//2,y+row*cell+cell//2;local c=boards[page][row+1]:sub(col+1,col+1)
  F.disc(cx,cy,radius,false);if c~='.'then F.ring(cx,cy,radius,c=='D',4)end
 end end
 if page==1 then F.ring(x+cell+cell//2,y-9-21,math.min(16,cell//3),false,4)
 elseif page==2 then for n=0,3*cell do smudge.rect(x+cell//2+n-3,y+3*cell+cell//2-n-3,7,7,true,true)end end
 local next={x=16,y=h-68,w=w-32,h=52};F.status(page==3 and 'GOT IT' or 'NEXT',next)
 return {page=page,pages=3,boxes={next}}
end

B.lesson=lesson
function B.preview(M,g,F,area,Q)
 local cell=math.min(30,(area.h-36)//6,area.w//7);local width,height=cell*7,cell*6;local x=area.x+(area.w-width)//2;local y=area.y+(area.h>height+38 and (area.h-height-38)//2 or 12)
 smudge.rect(x-4,y-4,width+8,height+8,true,true);smudge.rect_dither(x,y,width,height,false)
 for row=0,5 do for col=0,6 do local cx,cy=x+col*cell+cell//2,y+row*cell+cell//2;F.disc(cx,cy,cell*26//64,false);local p=g.b[(5-row)*7+col+1];if p>0 then F.ring(cx,cy,cell*26//64,p==2,3)end end end
 local col,row,dx,dy=winning(g);if col then for n=0,3 do local cx=x+(col+n*dx)*cell+cell//2;local cy=y+(5-row-n*dy)*cell+cell//2;local weight=math.max(2,cell//10);smudge.rounded_rect(cx-weight*2,cy-weight*2,weight*4,weight*4,weight,true,true,1)end end
 F.nativeText(g.status==3 and 'LAST GAME: A DRAW' or g.status==1 and 'LAST GAME: YOU WON' or 'LAST GAME: THEY WON',area.x,y+height+14,area.w,24,10)
end
return B
