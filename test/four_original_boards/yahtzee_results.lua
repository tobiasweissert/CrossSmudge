-- Controlled legal saved cards, completed by the actual production AI callback.
local Y=dofile('core.lua');local read=smudge.read_file;local saves={}
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,value)saves[path]=value;return true end
for outcome=1,4 do
 saves={};local g=Y.new(123456);for player=1,2 do for c=1,14 do g.cards[player][c]=0 end end
 if outcome==4 then for c=1,6 do g.cards[1][c]=5*c end;g.cards[1][7]=30;g.cards[1][8]=30;g.cards[1][12]=50 end;g.cards[2][1]=-1;g.cards[1][13]=outcome==1 and 30 or 0;g.cards[2][13]=outcome==2 and 30 or 0;g.turn=2;g.rolls=3;g.held=0;g.d={2,2,2,2,2};assert(Y.decode(Y.encode(g)))
 assert(dofile('storage.lua').save('1|'..Y.encode(g)));local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();on_update();on_draw();assert(Y.over(app.state()),'actual final AI take failed')
 local function shot(name,menu)
  on_draw();capture(name..'.pgm');local g=app.state();local values={g.turn,g.rolls,g.held};for i=1,5 do values[#values+1]=g.d[i]end;for player=1,2 do for c=1,14 do values[#values+1]=g.cards[player][c]end end
  local tail='';if menu then local R=app.records;tail='menu '..table.concat({R.played,R.won,R.best,R.yahtzees,R.face},' ')end
  assert(fixtureWrite(name..'.map',table.concat(values,' ')..'\n'..tail..'\n',false))
 end
 shot('result-'..outcome);assert(app.records.played==1 and app.records.best==Y.total(app.state().cards[1]))
 local w,h=smudge.get_bounds();on_touch(30,h-40,'tap');on_draw();assert(app.screen()=='menu');shot('record-'..outcome,true);on_exit()
 app=dofile('main.lua');on_init();on_draw();assert(app.records.played==1,'completed-game reopen duplicated record');on_exit()
end
