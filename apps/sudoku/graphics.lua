-- Jersey 25/OFL glyphs repacked losslessly from CrossPlay's 1-bit Epd cuts.
-- Load once; retain compact strings rather than per-glyph Lua tables.
local G={}
local fonts={}
function G.init()
 for _,size in ipairs({10,20,30}) do fonts[size]=assert(smudge.read_file('art/font'..size..'.bin',20000),'Missing CrossPlay font') end
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
return G
