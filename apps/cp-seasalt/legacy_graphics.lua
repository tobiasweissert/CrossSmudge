-- Original Jersey masks; strings load once, only on PSRAM-class Lua heaps.
local G={};local fonts={};local caps={[10]=13,[20]=25,[30]=38}
function G.init()for _,size in ipairs({10,20,30})do fonts[size]=assert(smudge.read_file('art/toybox_'..size..'.bin',65536))end end
local function record(font,c)c=math.max(32,math.min(255,c));return font:byte((c-32)*7+1,(c-32)*7+7)end
function G.width(text,size)local w=0;for i=1,#text do local _,_,advance=record(fonts[size],text:byte(i));w=w+advance end;return w end
function G.text(text,x,y,size,paper)
 text=tostring(text);local font=fonts[size]
 for i=1,#text do local w,h,advance,left,top,lo,hi=record(font,text:byte(i));if left>127 then left=left-256 end;if top>127 then top=top-256 end
  local offset=lo+hi*256;if w>0 and h>0 then smudge.bitmap(x+left,y+caps[size]-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false)end;x=x+advance
 end
end
function G.center(text,x,y,w,h,size,paper)G.text(text,x+(w-G.width(text,size))//2,y+(h-caps[size])//2,size,paper)end
function G.fit(text,width,size)while #text>0 and G.width(text,size)>width do text=text:sub(1,-2)end;return text end
return G
