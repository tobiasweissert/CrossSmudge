-- Jersey 25/OFL glyphs repacked losslessly from CrossPlay's 1-bit Epd cuts.
-- Load once; retain compact strings rather than per-glyph Lua tables.
local G={}
local fonts={}
local steps={}
local metrics={[10]={cap=13,asc=17,line=21},[20]={cap=25,asc=34,line=42},[30]={cap=38,asc=51,line=63},s10={cap=15,asc=21,line=27},s13={cap=20,asc=27,line=35}}
function G.init()
 for _,size in ipairs({10,20,30}) do fonts[size]=assert(smudge.read_file('art/font'..size..'.bin',20000),'Missing CrossPlay font') end
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
 local font=fonts[size];local cap=metrics[size].cap
 for i=1,#text do
  local w,h,advance,left,top,lo,hi=record(font,text:byte(i))
  if left>127 then left=left-256 end
  if top>127 then top=top-256 end
  local offset=lo+hi*256
  if w>0 then smudge.bitmap(x+left,y+cap-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false) end
  x=x+(steps[size] and i<#text and steps[size]:byte((text:byte(i)-32)*95+text:byte(i+1)-31)or advance)
 end
end
function G.center(text,x,y,w,h,size,paper)
 G.text(text,x+(w-G.width(text,size))//2,y+(h-(size==30 and 38 or size==20 and 25 or 13))//2,size,paper)
end

function G.lineCenter(text,b,size,paper)
 local line=size==30 and 63 or size==20 and 42 or 21
 local extra=size==30 and 13 or size==20 and 9 or 4
 local delta=b.h-line;local offset=delta<0 and -((-delta)//2) or delta//2
 G.text(text,b.x+(b.w-G.inkWidth(text,size))//2,b.y+offset+extra,size,paper)
end
function G.roundDither(x,y,w,h,r,dark)
 r=math.min(r,w//2,h//2)
 for row=0,h-1 do
  local dy=row<r and r-row or row>=h-r and row-(h-r-1) or 0
  local inset=dy>0 and r-math.floor(math.sqrt(r*r-dy*dy)) or 0
  smudge.rect_dither(x+inset,y+row,w-2*inset,1,dark)
 end
end
function G.fit(text,width,size)
 text=tostring(text):upper();while #text>0 and G.width(text,size)>width do text=text:sub(1,-2)end;return text
end
function G.serifInit()
 if fonts.s10 then return end
 for _,size in ipairs({10,13})do
  fonts['s'..size]=assert(smudge.read_file('art/serif'..size..'.bin',20000),'Missing original archive font')
  steps['s'..size]=assert(smudge.read_file('art/serif'..size..'-steps.bin',10000),'Missing original archive kerning')
 end
end
function G.nativeWidth(text,size)
 local pen,left,right=0,0,0
 for i=1,#text do
  local w,_,advance,bearing=record(fonts[size],text:byte(i))
  if bearing>127 then bearing=bearing-256 end
  left=math.min(left,pen+bearing);right=math.max(right,pen+bearing+w)
  pen=pen+(steps[size] and i<#text and steps[size]:byte((text:byte(i)-32)*95+text:byte(i+1)-31)or advance)
 end
 return right-left
end
local function truncDiv(value,divisor)
 local q=math.abs(value)//math.abs(divisor);return (value<0)~=(divisor<0)and -q or q
end
function G.native(text,b,size,align,paper,lines,centered)
 -- Original UI-count cut has no U+2026 glyph: GfxRenderer::truncatedText uses three periods.
 if size==20 and (lines or 1)==1 and G.nativeWidth(text,size)>b.w then
  local lo,hi=0,#text
  while lo<hi do local mid=(lo+hi+1)//2
   if G.nativeWidth(text:sub(1,mid)..'...',size)<b.w then lo=mid else hi=mid-1 end
  end
  text=text:sub(1,lo)..'...'
 end
 local m=metrics[size];local output={}
 if lines and lines>1 and G.nativeWidth(text,size)>b.w then
  local line=''
  for word in text:gmatch('%S+')do
   local next=line==''and word or line..' '..word
   if line~=''and G.nativeWidth(next,size)>b.w then output[#output+1]=line;line=word else line=next end
  end
  if line~=''then output[#output+1]=line end
 else output[1]=text end
 local y=b.y+(centered and truncDiv(b.h-m.line,2)or math.max(0,(b.h-#output*m.line)//2))
 for n=1,math.min(lines or 1,#output)do
  local run=output[n];local width=G.nativeWidth(run,size);local x=b.x
  if align=='center'then x=b.x+math.max(0,(b.w-width)//2)elseif align=='right'then x=b.x+math.max(0,b.w-width)end
  local py=y
  if align=='center'and #run==1 and run:match('%d')then
   local w,h,_,bearing,top=record(fonts[size],run:byte());if bearing>127 then bearing=bearing-256 end
   if w<=b.w and h<=m.line then
    x=b.x+(b.w-w)//2-bearing
    -- centredText hands the target a complete line box; ordinary text uses its original box.
    local by=centered and y or b.y;local bh=centered and m.line or b.h
    py=by+(bh-h)//2+top-m.asc
   end
  end
  G.text(run,x,py+m.asc-m.cap,size,paper)
  y=y+m.line
 end
end
function G.header(title,w,right,serif)
 smudge.rect(0,0,w,76,true,true);G.native(title,{x=16,y=0,w=w-32,h=76},30,'left',true)
 if right then
  local size=serif and 's13'or 10
  local width=G.nativeWidth(right,size)
  -- Native header right metadata sits on its baseline at the band's bottom.
  G.native(right,{x=w-16-width,y=serif and 34 or 43,w=width,h=serif and 35 or 33},size,'right',true)
 end
 smudge.rect(0,80,w,3,true,true)
end
local triangleX,triangleY={0,0,0},{0,0,0}
function G.triangle(x0,y0,x1,y1,x2,y2)
 -- Same integer intersections as original GfxRenderer::fillPolygon, with reusable three-point storage.
 local xs,ys=triangleX,triangleY;xs[1],xs[2],xs[3]=x0,x1,x2;ys[1],ys[2],ys[3]=y0,y1,y2
 for y=math.min(y0,y1,y2),math.max(y0,y1,y2)do
  local a,b;local j=3
  for i=1,3 do
   if ys[i]<y and ys[j]>=y or ys[j]<y and ys[i]>=y then
    local x=xs[i]+truncDiv((y-ys[i])*(xs[j]-xs[i]),ys[j]-ys[i])
    if not a then a=x else b=x end
   end
   j=i
  end
  if a and b then if a>b then a,b=b,a end;smudge.line(a,y,b,y,true)end
 end
end
function G.nativeLine(x0,y0,x1,y1,width)
 -- Original renderer centres thickness across the dominant axis; the host API offsets only downwards.
 local steep=math.abs(y1-y0)>math.abs(x1-x0)
 for offset=-(width//2),width-width//2-1 do
  smudge.line(x0+(steep and offset or 0),y0+(steep and 0 or offset),x1+(steep and offset or 0),y1+(steep and 0 or offset),true)
 end
end
return G
