assert(smudge.get_device()=='sticky'and math.maxinteger==2147483647)
local assetRead,outputWrite=smudge.read_file,smudge.write_file
local files,clock,failWrite={},123456,false
smudge.read_file=function(path,n,offset)if path:match('^art/')then return assetRead(path,n,offset)end;return files[path]and files[path]:sub((offset or 0)+1,(offset or 0)+n)end
smudge.write_file=function(path,s)if failWrite then return false end;files[path]=s;return true end
smudge.millis=function()return clock end
local traceFile
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do local native=smudge[name];smudge[name]=function(...)
 if name=='bitmap'then local x,y,w,h=...;local vw,vh=smudge.get_bounds();assert(x>=0 and y>=0 and x+w<=vw and y+h<=vh,'clipped glyph or sprite')end
 if traceFile then local row={name};for i,v in ipairs({...})do row[#row+1]=name=='bitmap'and i==5 and('bytes='..#v)or tostring(v)end;outputWrite(traceFile,table.concat(row,'\t')..'\n',true)end
 return native(...)
end end
local T={files=files}
function T.open()local app=dofile('main.lua');on_init();on_draw();T.app=app;return app end
function T.tap(b)assert(b,'missing drawn control');on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
function T.button(b)on_button(b,true);on_draw()end
function T.square(s)
 local g,l=T.app.state(),T.app.layout();local black=g.mode==2 or g.mode==3 and not g.position.whiteToMove
 local r,c=black and s//8 or 7-s//8,black and 7-s%8 or s%8
 on_touch(l.x+c*l.size+l.size//2,l.y+r*l.size+l.size//2,'tap');on_draw()
end
function T.setting(id)
 local a=T.app
 if a.state().home then T.button('confirm')end
 if not a.state().menu then T.tap(a.layout().gear)end
 for _=1,12 do if a.layout().buttons[id]then break end;T.button('down')end
 T.tap(a.layout().buttons[id])
end
function T.seed(fen,mode)
 local J=dofile('cp_journal.lua');assert(J.open('chess',11000,function()return true end):write((mode or 3)..' 2\n'..fen..'\n'));return T.open()
end
function T.shot(name)
 traceFile=name..'.trace';outputWrite(traceFile,'',false);on_draw();traceFile=nil;capture(name..'.pgm')
 local g=T.app.state();local N=dofile('notation.lua');local C=T.app.core
 local first=g.history[1]and C.parseFen(g.history[1])or g.position;local base=(first.fullmoveNumber-1)*2+(first.whiteToMove and 0 or 1)
 local reason=g.over and(g.over:find('threefold')and 1 or g.over:find('fifty')and 2 or 0)or 0
 local lines={C.fen(g.position),table.concat({g.mode,g.selected or -1,g.over and 1 or 0,#g.history,g.uiMode or g.mode,g.uiLevel or g.level,g.menuFromHome and 1 or 0,g.menuIndex or 1,g.restartPending and 1 or 0,T.app.hints()and 1 or 0,g.job and 1 or 0,reason,base},' ')}
 for i,fen in ipairs(g.history)do lines[#lines+1]=N.between(C,fen,g.history[i+1]or C.fen(g.position))end
 outputWrite(name..'.map',table.concat(lines,'\n')..'\n',false)
end
function T.fail(value)failWrite=value end
