local read=smudge.read_file;local saved={}
smudge.read_file=function(path,cap,offset)if path:sub(1,1)=='/'then local value=saved[path];return value and value:sub((offset or 0)+1,(offset or 0)+cap)end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saved[path]=value;return true end
local app=dofile('main.lua');on_init();on_draw();local l=app.layout()
assert(l.menu and #l.menu==2,'original fresh-deck front door missing')
capture('solitaire-menu.pgm')
local mode=l.menu[2];on_touch(mode.x+mode.w//2,mode.y+mode.h//2,'tap');on_draw()
assert(app.state().drawThree==false,'mode toggle changed current deal')
on_button('confirm',true);on_draw();assert(app.layout().cw and app.state().drawThree,'Deal did not honor selected draw mode')
on_touch(app.layout().x+3,app.layout().top+3,'tap');on_draw();assert(#app.state().piles[0]==21)
on_button('back',true);on_draw();assert(app.layout().menu and #app.layout().menu==3,'Back did not return to saved-deal front door')
capture('solitaire-menu-inprogress.pgm')
on_button('confirm',true);on_draw();assert(#app.state().piles[0]==21,'Pick It Up replaced saved deal')
local new=app.layout().menuButton;on_touch(new.x+3,new.y+3,'tap');on_draw();assert(app.layout().menu,'original header New did not return to the table menu')
on_exit();print('Original Solitaire front door/mode/Deal/Pick It Up actual input passed')
