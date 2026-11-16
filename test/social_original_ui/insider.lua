local read,write=smudge.read_file,smudge.write_file;local saved={};local now=123456
smudge.millis=function()return now end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local data=saved[path];return data and data:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,data)saved[path]=data;return true end
local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw();local g=app.state()
local function hit(action,value)
 for _,item in ipairs(app.view.hit)do if item.action==action and(value==nil or item.value==value)then local b=item.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing action '..tostring(action)..':'..tostring(value))
end
local function shot(name)
 local g=app.state();local fields={g.players,g.phase,g.seat,g.left,g.rounds,g.won,g.lost,g.timeout,g.result,(g.phase==6 and app.view.accused or g.phase==5 and app.view.chosen)or -1,app.view.help}
 for i,v in ipairs(fields)do fields[i]=tostring(v)end
 local word=g.word>0 and app.core.word(g.word)or'-'
 assert(write(name..'.state',table.concat(fields,' ')..'\n'..(g.roles==''and'-'or g.roles)..'\n'..word..'\n'..(g.recent==''and'-'or g.recent),false));capture(name..'.pgm')
end
shot('insider-menu-four');hit('players',1);shot('insider-menu-five');hit(2)
for seat=1,g.players do assert(g.phase==1 and g.seat==seat);shot('insider-pass-'..seat);hit(1);shot('insider-role-'..seat);hit(1)end
assert(g.phase==3);shot('insider-questions');now=now+75000;on_update();on_draw();shot('insider-questions-late')
hit(1);assert(g.phase==5);shot('insider-vote');local target=g.roles:find('I',1,true)or 0;hit('choose',target);shot('insider-vote-chosen');hit('accuse',target);assert(g.phase==6 and g.result==1 and g.rounds==1);shot('insider-result-win')
hit('menu');shot('insider-record');hit('help')
for page=1,5 do assert(app.view.help==page);shot('insider-help-'..page);hit('help-next')end
assert(app.view.help==0 and g.rounds==1);hit(2)
for seat=1,g.players do hit(1);hit(1)end
assert(g.phase==3);now=now+300001;on_update();on_draw();assert(g.phase==6 and g.result==3 and g.rounds==2);shot('insider-result-timeout')
hit('menu');on_update();on_exit();assert(smudge.get_orientation()==before)
app=dofile('main.lua');on_init();on_draw();assert(app.state().rounds==2 and app.state().won==1 and app.state().timeout==1);on_exit();assert(smudge.get_orientation()==before)
app=dofile('main.lua');on_init();on_draw();g=app.state();hit(2)
for seat=1,g.players do hit(1);hit(1)end
hit(1);local wrong=g.roles:find('I',1,true)and 0 or 1
if wrong>0 and g.roles:sub(wrong,wrong)=='M'then wrong=2 end
hit('choose',wrong);shot('insider-vote-wrong');hit('accuse',wrong);assert(g.result==2 and g.lost==1 and g.rounds==3);shot('insider-result-lost');hit('menu');on_exit()
local M=dofile('core.lua');local g=M.new(123456);g.players=8;g.rounds=65535;g.won=65535;g.lost=65535;g.timeout=65535;g.recent='1231123312311231';assert(M.valid(g));assert(dofile('storage.lua').save(M.encode(g)));app=dofile('main.lua');on_init();on_draw();shot('insider-menu-max');on_exit()
print('Actual Insider roles/vote/clock/timeout/record/tutorial/reopen callbacks passed')
