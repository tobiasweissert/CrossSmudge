-- Regression for fixed portrait coordinates clipping Stopwatch on Sticky landscape.
local w,h=smudge.get_bounds();local now=10000;local labels={};local rectangles={}
smudge.millis=function()return now end
smudge.thick_line=smudge.line
local rect,rounded,text=smudge.rect,smudge.rounded_rect,smudge.text
local function bounds(name,x,y,rw,rh)
 assert(x>=0 and y>=0 and x+rw<=w and y+rh<=h,
  name..' outside viewport '..table.concat({x,y,rw,rh,w,h},','))
 rectangles[#rectangles+1]={x=x,y=y,w=rw,h=rh}
end
smudge.rect=function(x,y,rw,rh,...)bounds('rect',x,y,rw,rh);return rect(x,y,rw,rh,...)end
smudge.rounded_rect=function(x,y,rw,rh,...)bounds('rounded_rect',x,y,rw,rh);return rounded(x,y,rw,rh,...)end
smudge.text=function(x,y,label,...)labels[#labels+1]=label;assert(y>=0 and y<h,'text below screen');return text(x,y,label,...)end
smudge.get_metrics=function()return{top_padding=12,header_height=50,button_hints_height=0,content_side_padding=20}end
local function tap(column)on_tap((column-.5)*w/4,h-30);on_draw()end
local function draw()labels={};rectangles={};on_draw()end
local function has(label)for _,s in ipairs(labels)do if s==label then return true end end;return false end
local function saved()return assert(smudge.read_file('stopwatch.dat',65536))end
dofile('main.lua');on_init();draw()
assert(has('Exit')and has('Start')and has('Split')and has('Reset'),'touch footer labels missing')
if h==800 then
 local found=false;for _,b in ipairs(rectangles)do if b.x==24 and b.y==352 and b.w==432 and b.h==380 then found=true end end
 assert(found,'portrait splits card redesigned')
end
tap(2);now=11000;tap(3);on_exit();assert(saved():find('1;1000\n1000;1000',1,true),'visible Split did not record running lap')
-- Restore the same saved elapsed/lap payload before continuing the stopwatch.
dofile('main.lua');on_init();draw();assert(has('Stop'))
for i=1,9 do now=now+1000;tap(3)end
on_tap(w-50,h-30);draw();assert(saved()=='0;0','visible Reset did not clear state')
tap(2);now=now+2000;tap(2);on_exit();assert(saved():match('^0;2000'),'visible Start/Stop/save flow changed')
print('PASS Stopwatch portrait preserved, landscape bounds, visible 60px touch footer, split/reset/resume')
