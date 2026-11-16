-- Legal core play creates save fixtures; production main loads them through its real journal.
local M=dofile('core.lua');local read,write=smudge.read_file,smudge.write_file;local saves={};local tick=123456
smudge.read_file=function(path,cap,offset)if saves[path]then return saves[path]:sub((offset or 0)+1,(offset or 0)+cap)end;if path:match('%.dat$')then return nil end;return read(path,cap,offset)end
smudge.write_file=function(path,value)saves[path]=value;return true end
local function random(n)tick=(tick~(tick<<13));tick=(tick~(tick>>17));tick=(tick~(tick<<5));return (tick&0x7fffffff)%n+1 end
local fixtures={}
for attempt=1,3000 do
 local g=M.new(123456);local plies=0
 while g.status==0 and plies<1000 do
  if M.cols==8 then local moves=M.moves(g);assert(#moves>0);assert(M.play(g,moves[random(#moves)]))
  else local available={}
   for c=1,M.cols do
    if M.cols==7 then if g.b[35+c]==0 then available[#available+1]=c end
    else if g.b[(g.turn-1)*9+(c-1)*3+3]==0 then available[#available+1]=c end end
   end
   assert(M.action(g,available[random(#available)]))
  end
  plies=plies+1
 end
 if g.status~=0 and not fixtures[g.status]then fixtures[g.status]=M.encode(g)end
 if fixtures[1]and fixtures[2]and fixtures[3]then break end
end
assert(fixtures[1]and fixtures[2]and fixtures[3],'bounded legal fixture generation did not find all outcomes')
for status=1,3 do
 saves={};local S=dofile('storage.lua');assert(S.save('1|'..fixtures[status]));local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();assert(app.state().status==status)
 capture('result-'..status..'.pgm');local g=app.state();local b={};for _,v in ipairs(g.b)do b[#b+1]=v end
 assert(fixtureWrite('result-'..status..'.map',table.concat(b)..'\n'..table.concat({g.turn,g.status,g.selected or 0,g.die or 0,g.idle or 0},' ')..'\n'..(g.history or'')..'\n',false))
 local l=app.layout();if status==1 then on_button('confirm',true)else on_touch(l.again.x+10,l.again.y+10,'tap')end
 on_draw();assert(app.state().status==0 and app.screen()=='board','Play Again did not immediately start a game');on_exit()
end
