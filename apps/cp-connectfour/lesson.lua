-- Original ConnectFourScreens.cpp three teaching fragments.
local lines={
 'TAP A COLUMN. YOUR DISC FALLS TO THE LOWEST FREE PLACE IN IT.',
 'FOUR IN A LINE WINS. ACROSS, UP, OR ON EITHER DIAGONAL LIKE THIS ONE.',
 'A FULL COLUMN STOPS TAKING DISCS, AND ITS SLOT AT THE TOP GOES GREY.'}
local boards={ {'....','....','.L..','.D..'}, {'...L','..LD','.LDD','LDLD'}, {'D...','L...','D...','LD..'} }
return function(page,F,w,h)
 page=math.max(1,math.min(3,page));F.header('HOW TO PLAY',page..' OF 3')
 F.lessonText(lines[page],16,119,w-32,h<600 and 10 or 20)
 local cell=h<600 and 40 or 64;local side=cell*4;local x=(w-side)//2;local y=h<600 and 218 or 389;local radius=math.floor(cell*26/64)
 smudge.rect(x-9,y-9,side+18,side+18,true,true);smudge.rect_dither(x,y,side,side,false)
 for row=0,3 do for col=0,3 do local cx,cy=x+col*cell+cell//2,y+row*cell+cell//2;local c=boards[page][row+1]:sub(col+1,col+1)
  smudge.circle(cx,cy,radius,true,false);if c~='.'then F.ring(cx,cy,radius,c=='D',4)end
 end end
 if page==1 then F.ring(x+cell+cell//2,y-9-21,math.min(16,cell//3),false,4)
 elseif page==2 then for n=0,3*cell do smudge.rect(x+cell//2+n-3,y+3*cell+cell//2-n-3,7,7,true,true)end end
 local next={x=16,y=h-68,w=w-32,h=52};F.pill(page==3 and 'GOT IT' or 'NEXT',next,true)
 return {page=page,pages=3,boxes={next}}
end
