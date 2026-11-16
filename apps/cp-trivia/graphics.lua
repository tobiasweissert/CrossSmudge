-- Immutable CrossPlay masks and exact differential 12.4 advance/kerning snaps.
-- Font strings load once per VM; no per-glyph tables or second framebuffer.
local F={fonts={}}
F.metrics={small={29,24,18},title={63,51,38},body={40,32,21}}
function F.init()
 for key,name in pairs({small='toybox_14',title='toybox_30',body='reading_serif_14'})do
  F.fonts[key]=assert(smudge.read_file('art/'..name..'.bin',32768),'Missing original Trivia font')
 end
 F.kerns=assert(smudge.read_file('art/reading_serif_14-kern.bin',2048),'Missing original Trivia kerning')
end
local function glyph(font,cp)
 if cp<32 or cp>255 then return 0,0,0,0,0,0 end
 local w,h,lo,hi,left,top,a,b=font:byte((cp-32)*8+1,(cp-32)*8+8)
 return w,h,lo+hi*256,left>127 and left-256 or left,top>127 and top-256 or top,a+b*256
end
local function kern(key,a,b)
 if key~='body' or a>255 or b>255 then return 0 end
 local l,r=F.kerns:byte(a+1),F.kerns:byte(256+b+1)
 if l==0 or r==0 then return 0 end
 local v=F.kerns:byte(512+(l-1)*30+r);return v>127 and v-256 or v
end
-- Original cuts cover ASCII (chrome), Latin-1 (prose). Preserve provider data;
-- display unavailable codepoints as readable U+ escapes instead of dropping it.
function F.visible(text,key)
 local font=F.fonts[key];local out={}
 for _,cp in utf8.codes(text)do
  local _,_,advance=glyph(font,cp)
  out[#out+1]=(cp==10 or cp==32 or advance>0) and utf8.char(cp) or string.format('[U+%04X]',cp)
 end
 return table.concat(out)
end
function F.width(text,key)
 -- GfxRenderer::getTextWidth delegates to EpdFont::getTextDimensions:
 -- measure ink bounds (including the origin), not the final pen advance.
 local prev,advance,x,minX,maxX=0,0,0,0,0;local font=F.fonts[key]
 for _,cp in utf8.codes(text)do
  if prev~=0 then x=x+(advance+kern(key,prev,cp)+8)//16 end
  local width,_,a,left=glyph(font,cp)
  minX=math.min(minX,x+left);maxX=math.max(maxX,x+left+width)
  advance=a;prev=cp
 end
 return maxX-minX
end
function F.text(text,x,y,key,white)
 local font=F.fonts[key];local asc=F.metrics[key][2];local prev,advance=0,0
 for _,cp in utf8.codes(text)do
  if prev~=0 then x=x+(advance+kern(key,prev,cp)+8)//16 end
  local w,h,a,left,top,offset=glyph(font,cp)
  if w>0 and h>0 then smudge.bitmap(x+left,y+asc-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),white or false,false)end
  prev,advance=cp,a
 end
end
function F.label(text,b,key,white,align)
 text=F.visible(text,key);local x=b.x;local width=F.width(text,key)
 if align=='right' then x=x+math.max(0,b.w-width)elseif align~='left' then x=x+math.max(0,(b.w-width)//2)end
 -- inkCentred plus native line-box centering for cap labels.
 F.text(text,x,b.y+(b.h+F.metrics[key][3])//2-F.metrics[key][2],key,white)
end
function F.wrap(text,width,key)
 text=F.visible(text,key);local lines,current={},''
 local function emit()lines[#lines+1]=current;current=''end
 for paragraph in (text..'\n'):gmatch('(.-)\n')do
  for word in paragraph:gmatch('%S+')do
   if current~='' and F.width(current..' '..word,key)>width then emit()end
   if F.width(word,key)>width then
    for _,cp in utf8.codes(word)do local char=utf8.char(cp)
     if current~='' and F.width(current..char,key)>width then emit()end
     current=current..char
    end
   else current=current=='' and word or current..' '..word end
  end
  if current~='' then emit()end
 end
 if #lines==0 then lines[1]=''end
 return lines
end
function F.prose(lines,b,key,white,center)
 local lineHeight=F.metrics[key][1];local y=b.y+math.max(0,(b.h-#lines*lineHeight)//2)
 for _,line in ipairs(lines)do F.text(line,center and b.x+math.max(0,(b.w-F.width(line,key))//2)or b.x,y,key,white);y=y+lineHeight end
end
return F
