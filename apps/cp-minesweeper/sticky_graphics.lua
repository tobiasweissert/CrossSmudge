-- Jersey 25/OFL glyphs repacked losslessly from CrossPlay's 1-bit Epd cuts.
-- Load once; retain compact strings rather than per-glyph Lua tables.
local G={}
local fonts={};local flags={}
function G.init()
 G.help=assert(smudge.read_file('help.txt',1024))
 for _,side in ipairs({20,36,40})do flags[side]=assert(smudge.read_file('art/flag'..side..'.raw',200))end
 for _,side in ipairs({9,21,24,26,30,34,44,60})do flags[side]=assert(smudge.read_file('art/nativeflag'..side..'.raw',600))end
 for _,size in ipairs({10,20,30}) do fonts[size]=assert(smudge.read_file('art/nativefont'..size..'.bin',20000),'Missing CrossPlay font') end
end
local function record(font,c)
 c=math.max(32,math.min(126,c));return string.byte(font,(c-32)*7+1,(c-32)*7+7)
end
function G.width(text,size)
 local width=0
 for i=1,#text do local _,_,advance=record(fonts[size],text:byte(i));width=width+advance end
 return width
end
-- Native EpdFont measures ink bounds, including the initial zero origin,
-- rather than the final glyph advance. Needed by the original status button.
function G.inkWidth(text,size)
 local pen,left,right=0,0,0
 for i=1,#text do
  local w,_,advance,bearing=record(fonts[size],text:byte(i))
  if bearing>127 then bearing=bearing-256 end
  left=math.min(left,pen+bearing);right=math.max(right,pen+bearing+w);pen=pen+advance
 end
 return right-left
end
function G.status(text,b)
 -- Original FreeInkUI button: 4px horizontal/2px vertical text inset,
 -- toybox_20 line advance 42, ascender 34 and capital top 25.
 smudge.rect(b.x,b.y,b.w,b.h,true,true)
 G.text(text,b.x+4+(b.w-8-G.inkWidth(text,20))//2,
  b.y+2+math.max(0,(b.h-4-42)//2)+34-25,20,true)
end
function G.text(text,x,y,size,paper)
 local font=fonts[size];local cap=size==30 and 38 or size==20 and 25 or 13
 for i=1,#text do
  local w,h,advance,left,top,lo,hi=record(font,text:byte(i))
  if left>127 then left=left-256 end
  local offset=lo+hi*256
  if w>0 then smudge.bitmap(x+left,y+cap-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false) end
  x=x+advance
 end
end
function G.center(text,x,y,w,h,size,paper)
 G.text(text,x+(w-G.width(text,size))//2,y+(h-(size==30 and 38 or size==20 and 25 or 13))//2,size,paper)
end

-- Original Toybox chrome; all font strings are loaded once per app VM.
function G.header(title,right)
 local w=smudge.get_bounds();title=string.upper(title);local size=G.width(title,30)<=w-32 and 30 or 20
 smudge.rect(0,0,w,76,true,true);G.text(title,16,(76-(size==30 and 38 or 25))//2,size,true)
 if right then G.text(right,w-16-G.inkWidth(right,20),36,20,true)end
 smudge.rect(0,80,w,3,true,true)
end
-- Original ToyboxScreen.h half-open scanline disc, not runtime midpoint circle.
function G.disc(x,y,r,black,dither)
 for dy=-r,r do local half=math.floor(math.sqrt(r*r-dy*dy))
  if half>0 then if dither then smudge.rect_dither(x-half,y+dy,2*half,1,black)else smudge.rect(x-half,y+dy,2*half,1,true,black)end end
 end
end
function G.ditherDisc(x,y,r,dark)G.disc(x,y,r,dark,true)end
function G.ring(x,y,r,filled,weight)
 G.disc(x,y,r,true);if not filled then G.disc(x,y,math.max(1,r-(weight or 3)),false)end
end
function G.bracket(x,y,w,h,arm,weight,paper)
 for _,dx in ipairs({0,w-arm}) do for _,dy in ipairs({0,h-weight}) do smudge.rect(x+dx,y+dy,arm,weight,true,not paper) end end
 for _,dx in ipairs({0,w-weight}) do for _,dy in ipairs({0,h-arm}) do smudge.rect(x+dx,y+dy,weight,arm,true,not paper) end end
end
function G.fit(text,width,size)
 text=string.upper(tostring(text or ''))
 while #text>0 and G.width(text,size)>width do text=text:sub(1,-2) end
 return text
end
function G.pill(text,b,selected,size)
 smudge.rounded_rect(b.x,b.y,b.w,b.h,math.min(20,b.h//2),selected,true,1)
 local s=size or 20;if G.width(string.upper(text),s)>b.w-16 then s=10 end
 G.center(G.fit(text,b.w-16,s),b.x,b.y,b.w,b.h,s,selected)
end
function G.inside(b,x,y)return b and x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h end
function G.wrap(text,width,size)
 local lines,current={},''
 for word in string.upper(text):gmatch('%S+') do
  local next=current=='' and word or current..' '..word
  if current~='' and G.width(next,size)>width then lines[#lines+1]=current;current=word else current=next end
 end
 if current~='' then lines[#lines+1]=current end;return lines
end
function G.die(x,y,side,value,paper,fixedPip)
 local pip=fixedPip or 9;local step=side//4;local cx,cy=x+2*step,y+2*step
 local positions=fixedPip and {13,side//2-pip//2,side-13-pip}
 local function p(dx,dy)smudge.rect(positions and x+positions[dx+2]or cx+dx*step-pip//2,positions and y+positions[dy+2]or cy+dy*step-pip//2,pip,pip,true,not paper)end
 if value%2==1 then p(0,0)end
 if value>=2 then p(-1,-1);p(1,1)end
 if value>=4 then p(1,-1);p(-1,1)end
 if value==6 then p(-1,0);p(1,0)end
end
-- Native DrawTarget text uses ink width and vertically centres the line box.
function G.nativeText(text,x,y,w,h,size,align,paper,inkCentre)
 local lines,current={},''
 if G.inkWidth(text,size)<=w then lines={text}end
 for word in (#lines>0 and ''or text):gmatch('%S+')do local candidate=current==''and word or current..' '..word
  if current~=''and G.inkWidth(candidate,size)>w then lines[#lines+1]=current;current=word else current=candidate end
 end
 if current~=''then lines[#lines+1]=current end
 local line=size==30 and 63 or size==20 and 42 or 21;local cap=size==30 and 38 or size==20 and 25 or 13
 local ascent=size==30 and 51 or size==20 and 34 or 17
 local top=inkCentre and y+(h-cap)//2 or y+math.max(0,(h-line*#lines)//2)+ascent-cap
 if #lines==1 and text:match('^%d$')and align~='left'and align~='right'then
  local gw,gh,_,bearing,gt=record(fonts[size],text:byte());if bearing>127 then bearing=bearing-256 end
  local ry=inkCentre and y+(h+cap)//2-ascent or y;local rh=inkCentre and line or h
  if gw<=w and gh<=rh then G.text(text,x+(w-gw)//2-bearing,ry+(rh-gh)//2+gt-cap,size,paper);return end
 end
 for i,v in ipairs(lines)do local left=align=='left'and x or align=='right'and x+w-G.inkWidth(v,size)or x+(w-G.inkWidth(v,size))//2
  G.text(v,left,top+(i-1)*line,size,paper)
 end
end
function G.lessonText(text,x,y,width,size)G.nativeText(text,x,y,width,120,size)end
function G.menu(items,selected,status)
 local w,h=smudge.get_bounds();local row=math.min(62,(h-150-4*(#items-1))//#items)
 local start=h-16-#items*row-(#items-1)*4;local boxes={}
 if status and status~='' then G.text(G.fit(status,w-32,10),16,128,10);smudge.rect(16,154,w-32,3,true,true)end
 for i,text in ipairs(items) do
  local b={x=16,y=start+(i-1)*(row+4),w=w-32,h=row};boxes[i]=b
  smudge.rect(b.x,b.y,b.w,b.h,i==selected,1,true);G.text(G.fit(text,b.w-24,20),b.x+12,b.y+(b.h-25)//2,20,i==selected)
 end
 return boxes
end
function G.flag(x,y,side)
 -- The source icon is a normal row-major bitmap; drawIcon's rotation is irrelevant.
 local n=side-2*(side//6);n=flags[n]and n or 20
 smudge.bitmap(x+(side-n)//2,y+(side-n)//2,n,n,flags[n])
end
local half={4,7,9,10,10,9,7,4}
local function mineBar(cx,cy,scale,paper,dx,dy,w,h)
 smudge.rect(cx+math.floor(dx*scale),cy+math.floor(dy*scale),math.max(1,math.floor(w*scale)),math.max(1,math.floor(h*scale)),true,not paper)
end
function G.mine(x,y,side,paper)
 local cx,cy=x+side//2,y+side//2
 local scale=math.min(1,(side-4)/34)
 for i=1,8 do mineBar(cx,cy,scale,paper,-half[i],-10+(i-1)*2+1,half[i]*2,2) end
 mineBar(cx,cy,scale,paper,-1,-16,3,6);mineBar(cx,cy,scale,paper,-1,11,3,6)
 mineBar(cx,cy,scale,paper,-16,-1,6,3);mineBar(cx,cy,scale,paper,11,-1,6,3)
end
local lessonLines={
 'A NUMBER COUNTS THE MINES TOUCHING IT, CORNERS INCLUDED.',
 'TAP TO DIG. HOLD TO PLANT A FLAG. THE BUTTON UNDER THE BOARD SWITCHES WHAT A TAP DOES.',
 'YOUR FIRST DIG IS ALWAYS SAFE, AND ALWAYS OPENS A SPACE.',
 'OPEN EVERY SAFE CELL AND THE FIELD IS CLEARED. FLAGS ARE NOTES: NONE ARE NEEDED TO WIN.',
 'TAP A NUMBER THAT HAS ALL ITS FLAGS AND THE REST OPENS. A WRONG FLAG ENDS IT.'}
local lessonFields={{'M..','.oM','..M'},{'oF.'},{'oo...','oo**.','ooo..','ooo*.'},{'ooooo','ooo*o','ooooo','ooFoo'},{'.F.','.o.','...'}}
function G.lesson(page,w,h)
 page=math.max(1,math.min(5,page));G.header('HOW TO PLAY',page..' OF 5')
 G.status(page==5 and'GOT IT'or'NEXT',{x=16,y=h-68,w=w-32,h=52})
 G.nativeText(lessonLines[page],16,h<600 and 107 or 119,w-32,h<600 and 80 or 150,h<600 and 10 or 20)
 local rows=lessonFields[page];local columns=#rows[1];local size=(page==3 or page==4)and(h<600 and 38 or 64)or(h<600 and 50 or 88)
 local band=h<600 and 209 or 279;local bottom=h-80;local height=#rows*size;local top=band+math.max(0,(bottom-band-height)//2);local left=(w-columns*size)//2
 local function mine(c,r)if r<1 or r>#rows or c<1 or c>columns then return false end;local v=rows[r]:sub(c,c);return v=='*'or v=='F'or v=='M'end
 for r,row in ipairs(rows)do for c=1,columns do local mark=row:sub(c,c);local x,y=left+(c-1)*size,top+(r-1)*size
  if mark~='o'and mark~='M'then smudge.rect_dither(x,y,size,size,false)end;smudge.rect(x,y,size,size,false,2,true)
  if mark=='F'then G.flag(x,y,size)
  elseif mark=='M'then smudge.rect(x,y,size,size,true,true);G.mine(x,y,size,true)
  elseif mark=='o'then local count=0;for dx=-1,1 do for dy=-1,1 do if(dx~=0 or dy~=0)and mine(c+dx,r+dy)then count=count+1 end end end
   if count>0 then G.nativeText(tostring(count),x,y,size,size,h<600 and 20 or 30,nil,false,true)end
  end
 end end
end

function G.result(M,g,w,h)
 G.header(g.status==2 and'CLEARED'or'BOOM');G.status('DONE',{x=16,y=h-68,w=w-32,h=52});G.status('PLAY AGAIN',{x=16,y=h-132,w=w-32,h=52})
 G.nativeText(g.status==2 and'YOU CLEARED THE FIELD'or'YOU HIT A MINE',16,149,w-32,60,20)
 local revealed,right=0,0;for _,v in ipairs(g.b)do if v&2~=0 and v&1==0 then revealed=revealed+1 end;if v&5==5 then right=right+1 end end
 G.nativeText(revealed..' OF 70 CELLS OPENED',16,229,w-32,60,20);G.nativeText(right..' OF 10 MINES FLAGGED',16,299,w-32,60,20)
end

function G.history(M,g,w,h)
 local room=h-312;local size=math.min(34,(room-34)//10);local width,height=size*8,size*10;local top=154+math.max(0,(room-height-34)//2);local left=(w-width)//2
 for row=0,9 do for col=0,7 do local x,y=left+col*size,top+row*size;local v=g.b[row*8+col+1]
  if v&1~=0 then smudge.rect(x,y,size,size,true,true)
  elseif v&2==0 then smudge.rect_dither(x,y,size,size,false);smudge.rect(x,y,size,size,false,1,true);if v&4~=0 then G.flag(x,y,size)end
  else smudge.rect(x,y,size,size,false,1,true);local n=M.adjacent(g,row*8+col+1,1);if n>0 then G.nativeText(tostring(n),x,y,size,size,10)end
  end
 end end
 smudge.rect(left-2,top-2,width+4,height+4,false,2,true)
 local lost=false;for _,v in ipairs(g.b)do if v&3==3 then lost=true;break end end
 G.nativeText(lost and'LAST GAME: BOOM'or'LAST GAME: CLEARED',16,top+height+10,w-32,24,10)
end

return G
