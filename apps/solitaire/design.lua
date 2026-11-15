-- Compact OFL Lato masks. The four immutable strings total under 50 KiB.
local U={fonts={}}
local function glyph(font,code)
 if code<32 or code>255 then code=63 end
 local w,h,advance,left,top,lo,hi=font:byte((code-32)*7+1,(code-32)*7+7)
 return w,h,advance,left>127 and left-256 or left,top>127 and top-256 or top,lo+hi*256
end
local function each(text)
 return text:gmatch('[%z\1-\127\194-\244][\128-\191]*')
end
local function code(char)
 local a,b=char:byte(1,2)
 return a<128 and a or a<224 and (a-192)*64+(b-128) or 63
end
function U.init()
 for _,key in ipairs({'b28','b36','b20','r20'}) do
  U.fonts[key]=assert(smudge.read_file('art/lato-'..key..'.bin',50000),'Missing UI font '..key)
 end
end
function U.width(text,key)
 local total=0;local font=U.fonts[key or 'r20']
 for char in each(tostring(text)) do local _,_,advance=glyph(font,code(char));total=total+advance end
 return total
end
function U.text(text,x,cy,key,align,white,gray)
 text=tostring(text);key=key or 'r20';local font=U.fonts[key]
 local width=U.width(text,key)
 if align=='center' then x=x-width//2 elseif align=='right' then x=x-width end
 local _,height,_,_,top=glyph(font,72);local baseline=math.floor(cy-top-height/2)
 for char in each(text) do
  local w,h,advance,left,y,offset=glyph(font,code(char))
  if w>0 and h>0 then smudge.bitmap(math.floor(x+left),baseline+y,w,h,font:sub(offset+1,offset+((w+7)//8)*h),white or false,gray or false) end
  x=x+advance
 end
end
function U.wrap(text,width,key,maxlines)
 local lines,current={},'';key=key or 'r20'
 local function push()lines[#lines+1]=current;current='';return maxlines and #lines>=maxlines end
 for paragraph in (tostring(text)..'\n'):gmatch('(.-)\n') do
  for word in paragraph:gmatch('%S+') do
   if current~='' and U.width(current..' '..word,key)>width then if push() then return lines end end
   if U.width(word,key)>width then
    for char in each(word) do
     if current~='' and U.width(current..char,key)>width then if push() then return lines end end
     current=current..char
    end
   else current=current=='' and word or current..' '..word end
  end
  if push() then return lines end
 end
 return lines
end
function U.round(x,y,w,h,r,fill,white)
 smudge.rounded_rect(math.floor(x),math.floor(y),math.floor(w),math.floor(h),r,fill,not white,2)
end
function U.tone(x,y,w,h,r,tone)
 x,y,w,h=math.floor(x),math.floor(y),math.floor(w),math.floor(h)
 for row=0,h-1 do
  local dy=row<r and r-row-.5 or row>=h-r and row-(h-r)+.5 or 0
  local inset=dy>0 and math.ceil(r-math.sqrt(math.max(0,r*r-dy*dy))) or 0
  if tone=='gray' or tone=='dark' then smudge.rect_dither(x+inset,y+row,w-2*inset,1,tone=='dark')
  else smudge.rect(x+inset,y+row,w-2*inset,1,true,tone=='black') end
 end
end
function U.tile(x,y,w,h,tone,border)
 U.tone(x,y,w,h,7,border or tone)
 U.tone(x+2,y+2,w-4,h-4,5,tone)
end
function U.button(label,x,y,w,h,selected)
 U.round(x,y,w,h,math.min(25,h//2),selected,false)
 U.text(label,x+w//2,y+h//2,'b20','center',selected)
end
function U.header(title,right,w)
 U.text(title,24,32,'b28');U.text(right or '',w-24,32,'r20','right',false,true)
 smudge.line(0,63,w-1,63,true)
end
function U.dash(x,y,w,h)
 for dx=0,w-1,10 do smudge.rect_dither(x+dx,y,math.min(5,w-dx),1,false);smudge.rect_dither(x+dx,y+h-1,math.min(5,w-dx),1,false) end
 for dy=0,h-1,10 do smudge.rect_dither(x,y+dy,1,math.min(5,h-dy),false);smudge.rect_dither(x+w-1,y+dy,1,math.min(5,h-dy),false) end
end
return U
