-- Jersey 25/OFL glyphs repacked losslessly from CrossPlay's 1-bit Epd cuts.
-- Load once; retain compact strings rather than per-glyph Lua tables.
local G={}
local fonts={}
local flags={}
function G.init()
 G.help=assert(smudge.read_file('help.txt',1024),'Missing help')
 fonts[20]=assert(smudge.read_file('art/font20.bin',20000),'Missing font20')
 collectgarbage('collect')
 fonts[30]=assert(smudge.read_file('art/font30.bin',20000),'Missing font30')
 collectgarbage('collect')
 for _,side in ipairs({20,36,40}) do flags[side]=assert(smudge.read_file('art/flag'..side..'.raw',200),'Missing flag') end
end
local function record(font,c)
 c=math.max(32,math.min(90,c));return string.byte(font,(c-32)*7+1,(c-32)*7+7)
end
function G.width(text,size)
 local width=0
 for i=1,#text do local _,_,advance=record(fonts[size],text:byte(i));width=width+advance end
 return width
end
function G.text(text,x,y,size,paper)
 local font=fonts[size];local cap=size==30 and 38 or 25
 for i=1,#text do
  local w,h,advance,left,top,lo,hi=record(font,text:byte(i))
  if left>127 then left=left-256 end
  local offset=lo+hi*256
  if w>0 then smudge.bitmap(x+left,y+cap-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false) end
  x=x+advance
 end
end
function G.center(text,x,y,w,h,size,paper)
 G.text(text,x+(w-G.width(text,size))//2,y+(h-(size==30 and 38 or 25))//2,size,paper)
end
function G.flag(x,y,side)
 -- The source icon is a normal row-major bitmap; drawIcon's rotation is irrelevant.
 local n=side>=60 and 40 or (side>=50 and 36 or 20)
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
function G.lesson(lesson,w,h,top)
   local page=1
   for text in G.help:gmatch('[^|]+') do
    if page==lesson then
     local y=top+24
     for line in text:gmatch('[^\n]+') do G.center(line,0,y,w,36,20);y=y+40 end
     break
    end
    page=page+1
   end
   G.center(tostring(lesson)..' OF 5',0,h-140,w,40,20)
   smudge.rect(16,h-68,w-32,52,true)
   G.center(lesson==5 and 'DONE' or 'NEXT',16,h-68,w-32,52,20,true)
end
return G
