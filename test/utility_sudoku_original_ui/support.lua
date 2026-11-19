assert(smudge.get_device()=='sticky','requires actual production Sticky identity')
local assetRead,outputWrite=smudge.read_file,smudge.write_file
local files,clock,failWrite={},123456,false
smudge.read_file=function(path,n)if path:match('^art/')then return assetRead(path,n)end;return files[path]and files[path]:sub(1,n)end
smudge.write_file=function(path,s)if failWrite then return false end;files[path]=s;return true end
smudge.millis=function()return clock end
local traceFile
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do
 local native=smudge[name];smudge[name]=function(...)
  if name=='bitmap'then local x,y,w,h=...;local vw,vh=smudge.get_bounds();assert(x>=0 and y>=0 and x+w<=vw and y+h<=vh,'glyph outside viewport '..x..','..y..' '..w..'x'..h)end
  if traceFile then local row={name};for i,v in ipairs({...})do row[#row+1]=name=='bitmap'and i==5 and ('bytes='..#v)or tostring(v)end;outputWrite(traceFile,table.concat(row,'\t')..'\n',true)end
  return native(...)
 end
end
local exited=false;smudge.exit=function()exited=true end
local T={files=files}
function T.exited()return exited end
function T.open()exited=false;local app=dofile('main.lua');on_init();on_draw();T.app=app;return app end
function T.shot(name)traceFile=name..'.trace';outputWrite(traceFile,'',false);on_draw();traceFile=nil;capture(name..'.pgm')
 local g,rows,screen,index=T.app.state()
 if type(g)=='string'then outputWrite(name..'.state.txt','screen='..tostring(screen)..' index='..tostring(index)..'\n'..g,false)
 elseif T.app.core and T.app.core.encode then outputWrite(name..'.state.txt',T.app.core.encode(g),false)end
 if type(g)=='table'and g.given then
  local n={};for i=1,81 do n[i]=tostring(g.notes[i])end
  outputWrite(name..'.map',table.concat(g.given)..'\n'..table.concat(g.solution)..'\n'..table.concat(g.value)..'\n'..table.concat(n,' ')..'\n'..table.concat({g.level,g.elapsed,g.hintsUsed,g.hardest,g.counts[g.level+1],g.best[g.level+1],g.newBest and 1 or 0,g.digit,#g.history},' ')..'\n',false)
 end
end
function T.tap(b)on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw()end
function T.action(action,value)on_draw();for _,hit in ipairs(T.app.hits())do if hit.action==action and(value==nil or hit.value==value)then T.tap(hit.box);return end end;error('missing rendered '..action)end
function T.button(b)on_button(b,true);on_draw()end
function T.fail(v)failWrite=v end
function T.advance(ms)clock=clock+ms;on_update();on_draw()end
