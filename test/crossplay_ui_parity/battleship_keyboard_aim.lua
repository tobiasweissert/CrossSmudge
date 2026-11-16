local read,write=smudge.read_file,smudge.write_file;local saves={}
smudge.read_file=function(path,n,offset)if saves[path]then return saves[path]:sub((offset or 0)+1,(offset or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,offset)end
smudge.write_file=function(path,value)saves[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();local l=app.layout();on_touch(l.view.x+10,l.view.y+10,'tap');on_draw()
l=app.layout();assert(app.state().phase==1 and app.state().turn==1)
on_button('confirm',true);on_draw();assert(app.state().sb==string.rep('0',100),'unarmed Confirm fired a hidden target');capture('aim-a1.pgm')
on_button('right',true);on_draw();capture('after-right.pgm');assert(read('aim-a1.pgm',400100)~=read('after-right.pgm',400100),'directional target moved without visible aim')
on_button('confirm',true);on_draw();capture('after-confirm.pgm')
assert(app.state().sb:sub(2,2)=='1'and app.state().sb:sub(1,1)=='0','Confirm did not fire the visibly aimed target')
assert(write('actual.state','last='..app.state().last..'\nshots='..app.state().sb..'\n',false))

on_exit();print('Physical unarmed Confirm aims only; navigation repaints target; Confirm fires the visible target')
