-- Actual immutable CrossPlay glyph masks; one compact string per cut, loaded once.
-- Ink widths and differential fixed-point advances match EpdFont/GfxRenderer.
local G={cap={[10]=13,[20]=25,[30]=38}}
local fonts={}
function G.init()for _,size in ipairs({10,20,30})do fonts[size]=assert(smudge.read_file('art/original'..size..'.bin',50000),'Missing original Jersey cut')end end
local function glyph(font,code)
 if code<32 or code>255 then code=63 end
 local w,h,a,b,left,top,lo,hi=font:byte((code-32)*8+1,(code-32)*8+8)
 return w,h,a+b*256,left>127 and left-256 or left,top>127 and top-256 or top,lo+hi*256
end
function G.clean(text)return tostring(text or '') end
-- Offline apps do not request network-only utf8 library; decode valid input locally.
local function codes(text)
 local i=1;return function()
  if i>#text then return end;local c=text:byte(i);i=i+1
  if c>=192 then local n=c<224 and 1 or c<240 and 2 or 3;c=c%(2^(6-n))
   for _=1,n do local b=text:byte(i)or 128;i=i+1;c=c*64+b%64 end
  end;return c
 end
end
function G.width(text,size)
 local x,minX,maxX,advance=0,0,0,nil
 for c in codes(G.clean(text))do
  if advance then x=x+(advance+8)//16 end
  local w,_,a,left=glyph(fonts[size],c);minX=math.min(minX,x+left);maxX=math.max(maxX,x+left+w);advance=a
 end;return maxX-minX
end
function G.text(text,x,y,size,paper)
 local font=fonts[size];local advance
 for c in codes(G.clean(text))do
  if advance then x=x+(advance+8)//16 end
  local w,h,a,left,top,offset=glyph(font,c)
  if w>0 and h>0 then smudge.bitmap(x+left,y+G.cap[size]-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false)end
  advance=a
 end
end
function G.center(text,x,y,w,h,size,paper)
 local xx=x+(w-G.width(text,size))//2;local line=size==30 and 63 or size==20 and 42 or 21;local asc=size==30 and 51 or size==20 and 34 or 17
 local yy=y+(h-line)//2+asc-G.cap[size]
 if #text==1 and text:match('^[0-9]$') then
  local gw,gh,_,left,top=glyph(fonts[size],text:byte())
  -- Original target applies its digit centering inside the inkCentred linebox.
  local asc=size==30 and 51 or size==20 and 34 or 17
  local line=size==30 and 63 or size==20 and 42 or 21
  xx=x+(w-gw)//2-left;yy=y+(h+G.cap[size])//2-asc+(line-gh)//2+top-G.cap[size]
 end
 G.text(text,xx,yy,size,paper)
end
return G
