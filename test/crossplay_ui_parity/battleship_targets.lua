local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end;if path:match('%.dat$')then return nil end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
tap(app.layout().view);local g=app.state()
local function contact(at)
 local l=app.layout();local x,y=l.x+(at-1)%10*l.cell+l.cell//2,l.y+(at-1)//10*l.cell+l.cell//2
 if on_tap then on_tap(x,y)end;on_touch(x,y,'tap');on_draw()
end
contact(1);assert(g.sb==string.rep('0',100),'first contact fired rather than aimed')
contact(1);assert(g.sb:sub(1,1)=='1'and g.turn==2,'second explicit contact on aimed target did not fire')
contact(2);assert(g.sb:sub(2,2)=='0','out-of-turn contact fired a shot')
on_update();on_draw();local before=g.sb
-- Compare actual shipped glyph calls for the original already-fired report.
local font=assert(read('art/font10.bin',20000));local expected={};local pen=16;local reportY=app.layout().reportY or 125
for c in ('ALREADY FIRED THERE'):gmatch('.')do
 local w,h,advance,left,top,lo,hi=font:byte((c:byte()-32)*7+1,(c:byte()-32)*7+7)
 if left>127 then left=left-256 end
 if w>0 then expected[#expected+1]={pen+left,reportY+13-top,w,h,font:sub(lo+hi*256+1,lo+hi*256+((w+7)//8)*h)}end
 pen=pen+advance
end
local calls={};local bitmap=smudge.bitmap
smudge.bitmap=function(x,y,w,h,data,...)
 if y>=reportY and y<reportY+14 then calls[#calls+1]={x,y,w,h,data}end
 return bitmap(x,y,w,h,data,...)
end
contact(1);assert(g.sb==before,'repeat shot changed the board')
assert(#calls==#expected,'original already-fired report is missing')
for i,want in ipairs(expected)do for field=1,5 do assert(calls[i][field]==want[field],'original already-fired report differs')end end
calls={};contact(2);local same=#calls==#expected
if same then for i,want in ipairs(expected)do for field=1,5 do if calls[i][field]~=want[field]then same=false end end end end
assert(not same,'new target kept stale already-fired report')
on_exit();print('Host contactpair/aim/secondtapfire/out-of-turn/repeatshot/originalreport passed')
