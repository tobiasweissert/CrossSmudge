-- Original CrossPlay Jersey cuts, losslessly repacked; immutable masks load once.
local F={cap={[10]=13,[14]=18,[20]=25,[30]=38}}
local fonts={}
function F.init()
 for _,size in ipairs({10, 14, 20, 30}) do fonts[size]=assert(smudge.read_file('art/font'..size..'.bin',50000),'Missing original Jersey cut') end
end
local function glyph(font,code)
 if code<32 or code>129 then code=63 end
 return font:byte((code-32)*7+1,(code-32)*7+7)
end
function F.clean(text)return tostring(text or '') end
function F.width(text,size)
 if text:find('[\128-\255]') then return smudge.text_width(text,size<=14 and 'small' or 'ui12') end
 local width=0;for i=1,#text do local _,_,advance=glyph(fonts[size],text:byte(i));width=width+advance end;return width
end
function F.text(text,x,y,size,paper)
 text=F.clean(text);x,y=math.floor(x),math.floor(y)
 if text:find('[\128-\255]') then smudge.text(x,y,text,size<=14 and 'small' or 'ui12',nil,'left',not paper);return end
 local font=fonts[size]
 for i=1,#text do
  local w,h,advance,left,top,lo,hi=glyph(font,text:byte(i));local offset=lo+hi*256
  if left>127 then left=left-256 end
  if w>0 and h>0 then smudge.bitmap(x+left,y+F.cap[size]-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false) end
  x=x+advance
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
return F
