-- Actual immutable CrossPlay glyph masks; one compact string per cut, loaded once.
-- Ink widths and differential fixed-point advances match EpdFont/GfxRenderer.
local F={cap={[10]=13,[14]=18,[20]=25,[30]=38}}
local fonts={}
function F.init()F.settings=assert(smudge.read_file('art/original-settings32.raw',128));for _,size in ipairs({10,14,20,30})do fonts[size]=assert(smudge.read_file('art/original'..size..'.bin',50000),'Missing original Jersey cut')end end
local function glyph(font,code)
 if code<32 or code>255 then code=63 end
 local w,h,a,b,left,top,lo,hi=font:byte((code-32)*8+1,(code-32)*8+8)
 return w,h,a+b*256,left>127 and left-256 or left,top>127 and top-256 or top,lo+hi*256
end
function F.clean(text)return tostring(text or '') end
-- Offline apps do not request network-only utf8 library; decode valid input locally.
local function codes(text)
 local i=1;return function()
  if i>#text then return end;local c=text:byte(i);i=i+1
  if c>=192 then local n=c<224 and 1 or c<240 and 2 or 3;c=c%(2^(6-n))
   for _=1,n do local b=text:byte(i)or 128;i=i+1;c=c*64+b%64 end
  end;return c
 end
end
-- Preserve imported UTF-8 bytes in storage; unsupported original cut glyphs
-- remain readable on screen as codepoints rather than disappearing.
function F.clean(text)
 local out={}
 for c in codes(tostring(text or ''))do
  local _,_,advance=glyph(fonts[20],c)
  if c>=32 and c<=255 and advance>0 then
   if c<128 then out[#out+1]=string.char(c)else out[#out+1]=string.char(192+c//64,128+c%64)end
  else out[#out+1]=string.format('[U+%04X]',c)end
 end;return table.concat(out)
end
function F.width(text,size)
 local x,minX,maxX,advance=0,0,0,nil
 for c in codes(F.clean(text))do
  if advance then x=x+(advance+8)//16 end
  local w,_,a,left=glyph(fonts[size],c);minX=math.min(minX,x+left);maxX=math.max(maxX,x+left+w);advance=a
 end;return maxX-minX
end
function F.text(text,x,y,size,paper)
 local font=fonts[size];local advance
 for c in codes(F.clean(text))do
  if advance then x=x+(advance+8)//16 end
  local w,h,a,left,top,offset=glyph(font,c)
  if w>0 and h>0 then smudge.bitmap(x+left,y+F.cap[size]-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false)end
  advance=a
 end
end
function F.center(text,b,size,paper)
 text=F.clean(text);F.text(text,b.x+(b.w-F.width(text,size))//2,b.y+(b.h-F.cap[size])//2,size,paper)
end
function F.fit(text,width,largest)
 text=F.clean(text)
 for _,size in ipairs({30,20,14,10}) do if size<=(largest or 30) and F.width(text,size)<=width then return size end end
 return 14
end
function F.wrap(text,width,size)
 local lines,current={},''
 for word in F.clean(text):gmatch('%S+') do
  if current~='' and F.width(current..' '..word,size)>width then lines[#lines+1]=current;current='' end
  if F.width(word,size)>width then
   for i=1,#word do local char=word:sub(i,i)
    if F.width(current..char,size)>width and current~='' then lines[#lines+1]=current;current='' end
    current=current..char
   end
  else current=current=='' and word or current..' '..word end
 end
 if current~='' then lines[#lines+1]=current end
 return lines
end
function F.block(text,b,largest,maxlines,paper)
 local chosen,lines=14,{}
 for _,size in ipairs({30,20,14,10}) do if size<=(largest or 30) then
  local wrapped=F.wrap(text,b.w,size)
  if #wrapped<=(maxlines or 3) and #wrapped*(F.cap[size]+8)-8<=b.h then chosen,lines=size,wrapped;break end
 end end
 if #lines==0 then lines=F.wrap(text,b.w,14) end
 local step=F.cap[chosen]+8;local y=b.y+(b.h-(#lines*step-8))//2
 for _,line in ipairs(lines) do F.center(line,{x=b.x,y=y,w=b.w,h=F.cap[chosen]},chosen,paper);y=y+step end
end
function F.box(x,y,w,h)return {x=math.floor(x),y=math.floor(y),w=math.floor(w),h=math.floor(h)} end
function F.rect(b,fill,paper,stroke)
 if fill then smudge.rect(b.x,b.y,b.w,b.h,true,not paper) else smudge.rect(b.x,b.y,b.w,b.h,false,stroke or 1,not paper) end
end
function F.round(b,r,fill,paper,stroke)smudge.rounded_rect(b.x,b.y,b.w,b.h,r,fill,not paper,stroke or 1)end
function F.corners(b,arm,weight,paper)
 for _,dx in ipairs({0,b.w-arm}) do for _,dy in ipairs({0,b.h-weight}) do smudge.rect(b.x+dx,b.y+dy,arm,weight,true,not paper) end end
 for _,dx in ipairs({0,b.w-weight}) do for _,dy in ipairs({0,b.h-arm}) do smudge.rect(b.x+dx,b.y+dy,weight,arm,true,not paper) end end
end
function F.header(title,w,right)
 smudge.rect(0,0,w,76,true,true);smudge.rect(0,80,w,3,true,true)
 local size=F.fit(title,w-32-(right and 120 or 0),30);F.text(title,16,(76-F.cap[size])//2,size,true)
 if right then F.text(right,w-16-F.width(right,20),26,20,true) end
end
function F.button(label,b,selected,size)
 F.round(b,10,selected,false,selected and 1 or 1)
 F.center(label,b,size or F.fit(label,b.w-24,20),selected)
end
function F.hit(list,b,action,value)
 list[#list+1]={box=b,action=action,value=value};return b
end
function F.at(list,x,y)
 for _,hit in ipairs(list) do local b=hit.box;if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then return hit end end
end
function F.pips(list,w,y,pages,current,action)
 local left=(w-pages*34)//2
 for page=1,pages do local x=left+(page-1)*34
  smudge.circle(x+17,y+22,5,true,page==current)
  if page~=current then smudge.circle(x+17,y+22,5,false,true) end
  F.hit(list,F.box(x,y,34,44),action,page)
 end
end
function F.sheet(text,b)
 local lines=F.wrap(text,b.w,20);local top=b.y+math.max(0,(b.h-#lines*42)//2)
 for i,line in ipairs(lines)do F.text(line,b.x,top+(i-1)*42+9,20)end
end
return F
