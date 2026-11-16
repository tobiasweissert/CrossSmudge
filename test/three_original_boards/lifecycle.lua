dofile('fixture-gameplay.lua')
local M=dofile('core.lua');local app=dofile('main.lua');on_init();on_draw()
local stats=app.records();assert(stats.wins+stats.losses+stats.draws==1,'reopen duplicated completion')
local encoded=M.encode(app.state());on_exit()
app=dofile('main.lua');on_init();on_draw();assert(M.encode(app.state())==encoded,'saved game changed on reopen')
assert(app.records().wins+app.records().losses+app.records().draws==1)
local w,h=smudge.get_bounds()
on_button('down',true);on_button('confirm',true);on_draw();assert(app.screen()=='nearby');capture('nearby.pgm')
-- Both existing local modes remain reachable from the honest runtime-service notice.
on_touch(30,h-126,'tap');on_draw();assert(app.screen()=='confirm');on_button('down',true);on_button('confirm',true);on_draw();assert(app.state().status==0)
local l=app.layout()
if M.cols==7 then on_touch(l.x+l.cell//2,l.y+10,'tap')elseif M.cols==3 then on_touch(l.x+l.cell//2,l.yours+10,'tap')else local m=M.moves(app.state())[1];local from,to=m:byte(1,2);on_touch(l.x+(from-1)%8*l.cell+10,l.y+(from-1)//8*l.cell+10,'tap');on_touch(l.x+(to-1)%8*l.cell+10,l.y+(to-1)//8*l.cell+10,'tap')end
local after=M.encode(app.state());on_update();assert(M.encode(app.state())==after,'local two-player unexpectedly ran AI')
on_button('back',true);on_draw();on_button('down',true);on_button('confirm',true);on_draw();on_touch(30,h-190,'tap');on_draw();assert(app.screen()=='confirm')
local prior=M.encode(app.state());on_button('confirm',true);on_draw();assert(M.encode(app.state())==prior,'cancel replaced saved game')
on_button('confirm',true);on_draw();assert(app.screen()=='board');l=app.layout()
if M.cols==7 then local c=1;while app.state().b[35+c]~=0 do c=c+1 end;on_touch(l.x+(c-1)*l.cell+10,l.y+10,'tap')
elseif M.cols==3 then local c=M.choose(app.state());local left=l.activeSeat==2 and(l.otherX or l.x)or l.x;local top=l.activeSeat==2 and l.theirs or l.yours;on_touch(left+(c-1)*(l.cell+l.gap)+10,top+10,'tap')
else local m=M.moves(app.state())[1];local from,to=m:byte(1,2);on_touch(l.x+(from-1)%8*l.cell+10,l.y+(from-1)//8*l.cell+10,'tap');on_touch(l.x+(to-1)%8*l.cell+10,l.y+(to-1)//8*l.cell+10,'tap')end
prior=M.encode(app.state())
local write=smudge.write_file;local exited=false;smudge.exit=function()exited=true end
smudge.write_file=function()return false end
on_button('back',true);on_button('back',true);assert(not exited,'failed save exited app');assert(M.encode(app.state())==prior)
smudge.write_file=write;on_exit()
