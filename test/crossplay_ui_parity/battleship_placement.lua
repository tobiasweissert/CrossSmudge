-- Native setup is a full editable fleet, Shuffle and explicit Ready.
local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)
 if path:match('^state%-')then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)saved[path]=value;return true end
local buttons=0;local rect=smudge.rect;local w,h=smudge.get_bounds()
smudge.rect=function(x,y,width,height,fill,black,...)
 if y==h-68 and width==(w-44)//2 and height==52 and fill and black then buttons=buttons+1 end
 return rect(x,y,width,height,fill,black,...)
end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
assert(buttons>=2,'Original placement must draw full fleet with Shuffle and Ready')
assert(app and app.state and app.layout,'native setup state/layout unavailable')
local g=app.state();local l=app.layout();local p=l.placement
assert(g.phase==0 and p and #p.board==100)
local hulls={};for i=1,100 do local ship=tonumber(p.board:sub(i,i));if ship>0 then hulls[ship]=(hulls[ship]or 0)+1 end end
for ship,length in ipairs({5,4,3,3,2})do assert(hulls[ship]==length,'native setup lacks full legal fleet')end
local function tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
local bow=p.board:find('1',1,true)
on_touch(l.x+(bow-1)%10*l.cell+l.cell//2,l.y+(bow-1)//10*l.cell+l.cell//2,'tap');on_draw()
assert(app.layout().placement.selected==1,'tap ship failed to select the drawn hull')
assert(g.phase==0 and g.ship==1 and g.a==string.rep('0',100),'editing committed before Ready')
tap(app.layout().rotate);assert(app.layout().placement.selected==0,'Shuffle kept stale selection')
local arranged=app.layout().placement.board
tap(app.layout().view)
assert(g.phase==1 and g.ship==6 and g.a==arranged,'Ready did not commit the drawn legal fleet')
assert(dofile('core.lua').valid(g),'Ready broke core validation')
on_exit();app=dofile('main.lua');on_init();on_draw()
assert(app.state().a==arranged and app.state().phase==1,'Ready fleet was not saved/reopened')
print('Original full fleet/select/Shuffle/Ready actual callbacks and save/reopen passed')
