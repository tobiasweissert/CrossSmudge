local Y=dofile('core.lua');local read=smudge.read_file;local saves={}
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)saves[path]=v;return true end
for variant=1,3 do
 saves={};local g=Y.new(123456);g.rolls=variant==1 and 1 or 3;g.d={3,3,3,3,3};g.cards[1][12]=50;g.cards[2][12]=50;if variant==3 then g.cards[1][3]=15;g.cards[2][3]=15;g.cards[1][14]=2;g.cards[2][14]=1 end;assert(Y.decode(Y.encode(g)));assert(dofile('storage.lua').save('1|'..Y.encode(g)))
 local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();local function shot(name)
  on_draw();capture(name..'.pgm');local g=app.state();local v={g.turn,g.rolls,g.held};for i=1,5 do v[#v+1]=g.d[i]end;for player=1,2 do for c=1,14 do v[#v+1]=g.cards[player][c]end end;assert(fixtureWrite(name..'.map',table.concat(v,' ')..'\n',false))
 end
 shot('joker-'..variant);local l=app.layout();local illegal=l.rows[variant==3 and 1 or 13];on_touch(illegal.x+10,illegal.y+10,'tap');on_draw();assert(app.state().turn==1,'Joker allowed forbidden category')
 local c=variant==3 and 9 or 3;local b=l.rows[c];on_touch(b.x+10,b.y+10,'tap');on_draw();assert(app.state().cards[1][c]>=0 and app.state().cards[1][14]==(variant==3 and 3 or 1));shot('joker-taken-'..variant);assert(app.records.face==3);on_exit()
end
