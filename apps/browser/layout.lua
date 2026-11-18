-- One visible page at a time. Anchors refer to semantic block/span/UTF-8 offsets.
local L={}
local function measure(text,font,style)return (smudge.browser_advance or smudge.text_width)(text,font,style)end
local function copy(a)return {b=a.b or 1,s=a.s or 1,p=a.p or 1}end
function L.font(run,delta)
 local n=math.max(0,math.min(2,(run.size or 0)+(delta or 0)))
 return n==2 and 'lexend16'or n==1 and 'lexend14'or 'ui12'
end
local function nextchar(s,p)return utf8.offset(s,2,p)or #s+1 end
function L.page(doc,anchor,width,height,delta)
 local a=copy(anchor or {});local rows={};local y=0;local line={runs={},height=0,width=0,y=0};local budget=0
 local function flush()
  if #line.runs==0 then return true end
  if y+line.height>height then return false end
  line.y=y;rows[#rows+1]=line;y=y+line.height
  line={runs={},height=0,width=0,y=y};return true
 end
 while a.b<=#doc.blocks do
  budget=budget+1;if budget>12000 then break end
  local b=doc.blocks[a.b]
  if b.image or b.form then
   if not flush()then break end
   local h=b.image and math.min(180,math.max(88,height//2))or 48
   if y+h>height and #rows>0 then break end
   rows[#rows+1]={y=y,height=h,image=b.image,form=b.form,block=b};y=y+h+8;a.b=a.b+1;a.s=1;a.p=1
  else
   local r=b.spans[a.s]
   if not r then
    if not flush()then break end
    y=y+math.max(6,b.style and b.style.margin or 0);a.b=a.b+1;a.s=1;a.p=1
   elseif a.p>#r.text then a.s=a.s+1;a.p=1
   else
    local previous=line.runs[#line.runs]
    if previous and previous.href and previous.href~=r.href then line.width=math.max(line.width,previous.x+44)end
    if r.href and (not previous or previous.href~=r.href)and width-line.width<44 then if not flush()then break end end
    local start=copy(a);local font=L.font(r,delta);local style=r.bold and 'bold'or 'regular'
    local p=a.p;local c=r.text:sub(p,p);local last
    if c=='\n' then a.p=p+1;if not flush()then a=start;break end
    else
     local space=c:match('%s')~=nil
     last=p
     while last<=#r.text and r.text:sub(last,last)~='\n'and (r.text:sub(last,last):match('%s')~=nil)==space do last=nextchar(r.text,last)end
     local text=r.text:sub(p,last-1)
     if space and not r.pre then text=' 'end
     if #line.runs==0 and space and not r.pre then a.p=last
     else
      local w=measure(text,font,style)
      if w>width and (not space or r.pre) then
       -- Long tokens split with a linear scan of individual glyph widths, then
       -- verify the actual prefix in the same font/style context used to draw.
       local e=p;local sum=0;local available=width-line.width
       while e<last do local n=nextchar(r.text,e);local cw=measure(r.text:sub(e,n-1),font,style);if sum+cw>available and e>p then break end;sum=sum+cw;e=n;if sum>=available then break end end
       text=r.text:sub(p,e-1);last=e;w=measure(text,font,style)
       while w>available and last>nextchar(r.text,p)do last=utf8.offset(r.text,-1,last);text=r.text:sub(p,last-1);w=measure(text,font,style)end
      end
      -- Prose follows native leading; only actionable rows reserve touch height.
      local h=math.max(smudge.line_height(font)+4,r.href and 44 or 0)
      if line.width+w>width and #line.runs>0 then
       if not flush()then a=start;break end
      elseif y+math.max(line.height,h)>height then a=start;break
      else
       local previous=line.runs[#line.runs]
       if previous and previous.font==font and previous.bold==r.bold and previous.href==r.href and previous.block==b then previous.text=previous.text..text
       else line.runs[#line.runs+1]={text=text,x=line.width,font=font,bold=r.bold,href=r.href,block=b}end
       line.width=line.width+w;line.height=math.max(line.height,h);a.p=last
      end
     end
    end
   end
  end
  if y>=height then break end
 end
 flush()
 return rows,a,a.b>#doc.blocks
end
function L.find(doc,query,start)
 query=query:lower();if query==''then return end
 for i=start or 1,#doc.blocks do
  -- The model bounds all text to 64KiB. Join only one semantic block;
  -- folding ASCII preserves the byte coordinates of the original spans.
  local spans=doc.blocks[i].spans or {};local parts={}
  for j,r in ipairs(spans)do parts[j]=r.text end
  local p=table.concat(parts):lower():find(query,1,true)
  if p then
   for j,r in ipairs(spans)do
    if p<=#r.text then return{b=i,s=j,p=utf8.offset(r.text,0,p)}end
    p=p-#r.text
   end
  end
 end
end
return L
