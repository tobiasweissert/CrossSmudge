local Y=dofile('core.lua');local read=smudge.read_file;local saves={};local fail=false
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)if fail then return false end;saves[path]=v;return true end
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local turns=0
while not Y.over(app.state())do
 local g=app.state()
 if g.turn==1 then
  local l=app.layout();on_touch(l.roll.x+10,l.roll.y+10,'tap');on_draw();assert(g.rolls>0)
  local c;for i=1,13 do if Y.allowed(g.cards[1],g.d,i)then c=i;break end end;assert(c)
  local row=app.layout().rows[c];assert(row);on_touch(row.x+10,row.y+row.h//2,'tap');on_draw();assert(g.cards[1][c]>=0);turns=turns+1
 else on_update()end
 assert(turns<=13)
end
on_draw();capture('complete.pgm');assert(turns==13 and app.records.played==1);local encoded=Y.encode(app.state());on_exit();app=dofile('main.lua');on_init();on_draw();assert(Y.encode(app.state())==encoded and app.records.played==1)
-- Core failure cannot report a successful exit or replace the last saved game.
on_button('confirm',true);on_draw();on_button('confirm',true);on_draw();fail=true;on_exit();fail=false;app=dofile('main.lua');on_init();on_draw();assert(Y.encode(app.state())==encoded)
on_exit()
