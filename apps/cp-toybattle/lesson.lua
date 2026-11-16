local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_lesson.lua')end
local data=dofile('lesson_data.lua');local L={};local cachedPage,pixels
function L.draw(page,F,w,h,B)
 page=math.max(1,math.min(8,page));local p=data.pages[page];F.header(p[1],page..'/8')
 local height=page==7 and 601 or 417
 if cachedPage~=page then pixels=assert(smudge.read_file('art/help-diagram-'..page..'.raw',56*height+1));cachedPage=page end
 smudge.bitmap(16,119,448,height,pixels)
 if p[2]~=''then F.native(p[2],{x=16,y=552,w=448,h=168},20,'center',false,4)end
 local boxes={{x=16,y=732,w=218,h=52},{x=246,y=732,w=218,h=52}}
 for i,b in ipairs(boxes)do smudge.rounded_rect(b.x,b.y,b.w,b.h,20,true,i==2,1);smudge.rounded_rect(b.x,b.y,b.w,b.h,20,false,true,1)
  F.native(i==1 and(page==1 and'BACK'or'PREV')or(page==8 and'PLAY'or'NEXT'),{x=b.x+4,y=b.y+2,w=b.w-8,h=b.h-4},i==1 and 10 or 20,'center',i==2)
 end
 return boxes
end
return L
