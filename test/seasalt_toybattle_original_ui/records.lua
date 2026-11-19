assert(APP=='seasalt'and smudge.get_device()=='sticky');local R=dofile('records.lua');R.played=65535;R.won=65535;R.match='1:1:0:0:0:0';R.dirty=true;assert(R.save())
for pass=1,2 do
 local app=dofile('main.lua');on_init();on_draw();local name=pass==1 and'long-records'or'saved-long-records';capture(name..'.pgm');local g,v=app.state(),app.view;assert(v.record.played==65535 and v.record.won==65535);assert(smudge.write_file(name..'.menu',table.concat({v.saved and 1 or 0,v.record.played,v.record.won,g.round,g.score[1],g.score[2]},' ')..'\n',false));on_exit()
end
