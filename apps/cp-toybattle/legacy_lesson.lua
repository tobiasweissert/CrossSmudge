local data=dofile('lesson_data.lua');local L={}
local meanings={'JOKER: COVERS ANY, COVERED BY ANY','DRAWS TWO FROM YOUR RESERVE','LETS YOU PLACE A SECOND TROOP','REMOVES A TROOP NEXT TO IT','LANDS ANYWHERE, PATH OR NOT','SHOOTS A TROOP OFF THEIR RACK','DRAWS ONE FROM YOUR RESERVE','THE STRONGEST. NO EFFECT'}
function L.draw(page,F,w,h,B)
 page=math.max(1,math.min(8,page));local p=data.pages[page];F.header(p[1],page..'/8');local landscape=w>h
 local picture={x=16,y=119,w=landscape and(w-48)//2 or w-32,h=landscape and h-215 or h-390}
 if p[3]then
  local nodes=data.nodes[p[3]];local side=math.max(26,math.min(60,picture.h//9));local pad=side//2+10;local points={}
  for i,n in ipairs(nodes)do points[i]={picture.x+pad+n[1]*(picture.w-2*pad)//1000,picture.y+pad+n[2]*(picture.h-2*pad)//1000}end
  for i,link in ipairs(data.links)do local a,b=points[link[1]],points[link[2]];smudge.line(a[1],a[2],b[1],b[2],true,page==5 and(i==11 or i==13)and 5 or 1)end
  local mx,my=picture.x+picture.w//2,picture.y+pad+430*(picture.h-2*pad)//1000;smudge.rounded_rect(mx-22,my-11,44,22,8,true,false,1);smudge.circle(mx-9,my,7,true,true);smudge.circle(mx+8,my,7,true,true)
  if page==1 or page==6 then F.bracket(mx-26,my-15,52,30,7,3)end
  for i,n in ipairs(nodes)do local x,y=points[i][1]-side//2,points[i][2]-side//2;local hq=n[3]=='H'or n[3]=='E';local radius=side//6
   smudge.rect_dither(x,y,side,side,n[4]~=2);smudge.rounded_rect(x,y,side,side,radius,false,true,hq and 5 or n[6]>0 and 5 or 3)
   if n[3]~=' 'then local ours=not hq and n[4]==0;local inset=hq and side//5 or 0;smudge.rounded_rect(x+inset,y+inset,side-2*inset,side-2*inset,math.max(0,radius-1),true,ours,1);F.center(n[3],x,y,side,side,side>=44 and 20 or 10,ours)end
   if n[6]>0 then smudge.circle(x+side-2,y+2,side//5+4,true,false);smudge.circle(x+side-2,y+2,side//5+2,true,true);B.glyph(x+side-2,y+2,side//5+3,n[6],true)end
   if n[5]==4 then F.bracket(x-6,y-6,side+12,side+12,7,3)
   elseif n[5]>0 then local cx,cy=x+side,y;smudge.circle(cx,cy,10,true,false);smudge.circle(cx,cy,8,true,true)
    if n[5]==1 then smudge.line(cx-4,cy,cx-1,cy+4,false,3);smudge.line(cx-1,cy+4,cx+4,cy-4,false,3)else smudge.line(cx-4,cy-4,cx+4,cy+4,false,3);smudge.line(cx+4,cy-4,cx-4,cy+4,false,3)end
   end
  end
 else
  local cols=landscape and 2 or 1;local rows=8//cols;local row=(h-210)//rows;local width=(w-32)//cols
  for k=1,8 do local col=(k-1)//rows;local yy=119+(k-1)%rows*row;local xx=16+col*width;local cardH=math.min(64,row-6);local cardW=cardH*48//64
   smudge.rect_dither(xx,yy,cardW,cardH,false);smudge.rounded_rect(xx,yy,cardW,cardH,6,false,true,2);F.center(k==1 and '*'or tostring(k-1),xx,yy,cardW,(k==1 or k==8)and cardH or cardH*3//5,cardH>=50 and 20 or 10);if k>1 and k<8 then B.troopMark(xx+cardW//2,yy+cardH*3//4,k,F)end
   local y=yy+3;for _,line in ipairs(F.wrap(meanings[k],width-cardW-20,10))do F.text(line,xx+cardW+12,y,10);y=y+20 end
  end
 end
 if p[2]~=''then F.lessonText(p[2],landscape and w//2+16 or 16,landscape and 135 or h-250,landscape and(w-48)//2 or w-32,landscape and 10 or 20)end
 local boxes={{x=16,y=h-68,w=(w-44)//2,h=52},{x=(w+12)//2,y=h-68,w=(w-44)//2,h=52}};F.pill(page==1 and 'BACK'or'PREV',boxes[1],false);F.pill(page==8 and 'PLAY'or'NEXT',boxes[2],true)
 return boxes
end
return L
