local w,h=480,800;local initialW,initialH=smudge.get_bounds();local original=initialW>initialH and 'landscape' or 'portrait';local orientation=original
smudge.get_orientation=function()return orientation end
smudge.set_orientation=function(value)orientation=value;w,h=value=='portrait' and 480 or 800,value=='portrait' and 800 or 480 end
smudge.get_bounds=function()return w,h end
local oldRect,oldBitmap=smudge.rect,smudge.bitmap
local oldText=smudge.text
smudge.text=function(x,y,text,font,style,align,color)assert(align==nil or type(align)=='string');return oldText(x,y,text,font,style,align,color)end
smudge.rect=function(x,y,bw,bh,...)assert(x>=0 and y>=0 and bw>0 and bh>0 and x+bw<=w and y+bh<=h,table.concat({x,y,bw,bh,w,h},','));return oldRect(x,y,bw,bh,...)end
smudge.bitmap=function(x,y,bw,bh,data,...)
 assert(#data==((bw+7)//8)*bh);assert(x>=0 and y>=0 and x+bw<=w and y+bh<=h,table.concat({x,y,bw,bh,w,h},','));return oldBitmap(x,y,bw,bh,data,...)
end
local note=smudge.read_file('note1.md',3073)
local calculator=smudge.read_file('decimal.lua',1)~=nil
if not calculator then assert(smudge.write_file('note1.md','Groceries\n- [ ] Milk\n- [X] Eggs\n- [ ] '..string.rep('long ',70)..'\n- [ ] Caf\195\169\n')) end
local api=dofile('main.lua');assert(api);on_init();on_draw();assert(orientation=='portrait')
local function tapKey(i)
 local b=api.cells()[i];on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()
end
local function touch(kind,value)
 for _,v in ipairs(api.hits()) do if v.action==kind and (value==nil or v.value==value) then local b=v.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing hit '..kind..':'..tostring(value))
end
if calculator then
 tapKey(9);tapKey(16);tapKey(10);tapKey(20);assert(api.core.display(api.state())=='9')
 tapKey(20);assert(api.core.display(api.state())=='14')
 tapKey(1);tapKey(6);tapKey(4);tapKey(18);tapKey(20);assert(api.core.display(api.state())=='DIVIDE BY 0')
 tapKey(1);tapKey(6);tapKey(2);assert(api.core.display(api.state())=='0')
 tapKey(1);tapKey(5);tapKey(17);assert(api.core.display(api.state())=='-7')
 on_long_press(100,100);on_draw();on_touch(100,350,'tap');on_draw()
 on_button('right',true);on_button('confirm',true);on_draw();assert(api.core.decode(api.core.encode(api.state())))
else
 on_swipe('up');on_draw();touch('library',8);on_button('back',true);on_draw();on_button('back',true);on_draw()
 on_swipe('down');on_draw();touch('library',1)
 touch('line',2);local text=api.state();assert(text:find('- %[x%] Milk'))
 local p=api.pages();assert(#p>=2);on_swipe('up');on_draw();local _,current=api.pages();assert(current==2)
 on_swipe('down');on_draw();touch('menu');touch('option',2);touch('confirm',1);assert(api.state()==text)
 touch('menu');touch('option',2);touch('confirm',2);assert(not api.state():find('Milk'));assert(not api.state():find('Eggs'))
 touch('menu');touch('option',5);touch('confirm',2);assert(api.state():find('Milk'));assert(smudge.read_file('note1.md',3073):find('%[ %] Milk'))
 touch('menu');touch('option',6);touch('back');on_draw();on_button('back',true);on_draw()
end
on_exit();assert(orientation==original)
print('utility original-layout/input/storage flow OK')
