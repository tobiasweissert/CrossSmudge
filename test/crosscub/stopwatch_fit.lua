-- Owner 818b0bef Stopwatch 1.0.3 / unified_ui v1 contract, not legacy 1.0.1 geometry.
local w,h=smudge.get_bounds();local now=10000;local labels={};local rectangles={}
smudge.millis=function()return now end
smudge.thick_line=smudge.line
-- Explicit bounded font boundary: src/main.cpp built-ins and LuaRunner::l_lineHeight.
-- Font yAdvance values, not point sizes or a no-op API.
local heights={small=20,ui10=25,ui12=30,lexend16=42}
smudge.line_height=function(font)return assert(heights[font],'unmodeled Stopwatch font '..tostring(font))end
assert(smudge.line_height('ui10')==25 and smudge.line_height('lexend16')==42)
local rect,rounded,text=smudge.rect,smudge.rounded_rect,smudge.text
local function bounds(name,x,y,rw,rh)
 assert(x>=0 and y>=0 and x+rw<=w and y+rh<=h,
  name..' outside viewport '..table.concat({x,y,rw,rh,w,h},','))
 rectangles[#rectangles+1]={x=x,y=y,w=rw,h=rh}
end
smudge.rect=function(x,y,rw,rh,...)bounds('rect',x,y,rw,rh);return rect(x,y,rw,rh,...)end
smudge.rounded_rect=function(x,y,rw,rh,...)bounds('rounded_rect',x,y,rw,rh);return rounded(x,y,rw,rh,...)end
smudge.text=function(x,y,label,font,bold,align,...)
 labels[#labels+1]=label
 local width=smudge.text_width(label,font,bold)
 local left=x;if align=='center' then left=x-width//2 elseif align=='right' then left=x-width end
 assert(left>=0 and left+width<=w and y>=0 and y+smudge.line_height(font)<=h,'text outside screen: '..label)
 return text(x,y,label,font,bold,align,...)
end
local function draw()labels={};rectangles={};on_draw()end
local function has(label)for _,s in ipairs(labels)do if s==label then return true end end;return false end
local function saved()return assert(smudge.read_file('stopwatch.dat',65536))end
local function hasrect(x,y,rw,rh)
 for _,b in ipairs(rectangles)do if b.x==x and b.y==y and b.w==rw and b.h==rh then return true end end
 return false
end
-- Independently pinned owner screen_layout and U.footer (pad14/gap8/height48).
local card=h==800 and {14,240,452,492} or {320,112,466,300}
local footer=w==480 and {{14,107},{129,107},{244,107},{359,107}} or {{14,187},{209,187},{404,187},{599,187}}
local function tap(column)
 local r=footer[column];on_tap(r[1]+r[2]//2,h-36);draw()
end
dofile('main.lua');on_init();draw()
assert(has('Exit')and has('Start')and has('Split')and has('Reset'),'touch footer labels missing')
assert(hasrect(table.unpack(card)),'owner 1.0.3 splits card geometry changed')
for _,r in ipairs(footer)do assert(hasrect(r[1],h-60,r[2],48),'footer draw/hit geometry changed')end
-- Disabled Split and footer gaps/outside edges must not activate Start.
tap(3);assert(has('Start'))
for _,p in ipairs({{footer[2][1]-1,h-36},{footer[2][1]+footer[2][2],h-36},{footer[2][1],h-61},{footer[2][1],h-12}})do
 on_tap(p[1],p[2]);draw();assert(has('Start'),'outside footer hit activated Start')
end
on_tap(footer[2][1],h-60);draw();assert(has('Stop'))
now=11000;tap(3);on_exit();assert(saved():find('1;1000\n1000;1000',1,true),'visible Split did not record running lap')
-- Restore the same saved elapsed/lap payload before continuing the stopwatch.
dofile('main.lua');on_init();draw();assert(has('Stop') and has('1 LAPS'))
for i=1,9 do now=now+1000;tap(3)end
assert(saved():find('1000;10000',1,true),'resumed lap/total continuity changed')
on_tap(footer[4][1]+footer[4][2]-1,h-13);draw();assert(saved()=='0;0','visible Reset did not clear state')
tap(2);now=now+2000;tap(2);on_exit();assert(saved():match('^0;2000'),'visible Start/Stop/save flow changed')
-- Shared timer draw/hit rectangle remains {14,112,landscape292/portrait452,100}.
on_tap(14,112);draw();assert(has('Stop'),'timer draw/hit start rectangle changed')
now=now+1000;on_tap(w>h and 305 or 465,211);draw();on_exit()
assert(saved()=='0;3000','timer hit resume/stop elapsed persistence changed')
print('PASS Stopwatch owner 1.0.3 both-orientation geometry, footer hit edges/gaps, split/reset/resume elapsed persistence')
for _,bad in ipairs({'not a stopwatch','1;100\n200;100','0;2147483648','0;1\n0;2'})do
 assert(smudge.write_file('stopwatch.dat',bad));dofile('main.lua');on_init();draw()
 assert(has('Saved data preserved'),'malformed save warning missing')
 tap(2);tap(3);tap(4);on_exit();assert(saved()==bad,'malformed saved bytes replaced')
end
print('PASS Stopwatch malformed saved bytes retained after Start/Split/Reset/exit; bounded text and rectangles')
