assert(smudge.get_device()=='sticky');local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw();assert(smudge.get_orientation()=='portrait');capture('menu.pgm')
local function tap(op,value)
 local v=APP=='seasalt'and app.view or app.view();local controls=APP=='seasalt'and v.controls or v.controls
 for _,c in ipairs(controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing '..op..' '..tostring(value))
end
if APP=='seasalt'then
 tap('help');for page=1,6 do capture('help-'..page..'.pgm');tap('lesson')end;tap('new')
else
 tap('home',3);for page=1,8 do capture('help-'..page..'.pgm');tap('next')end;capture('setup.pgm');tap('maps');capture('maps-1.pgm');on_button('page_forward',true);on_draw();capture('maps-2.pgm');tap('map',6);tap('maps');tap('map',0);tap('start')
end
capture('board.pgm');dofile(APP=='seasalt'and'export_seasalt.lua'or'export_toy.lua')(app,'board');on_exit();assert(smudge.get_orientation()==before)
