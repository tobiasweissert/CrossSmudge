dofile('fixture-gameplay.lua')
local path='native-record-a.dat';local raw=assert(smudge.read_file(path,1201));local board=assert(raw:match('\n%d+ %d+ %d+\n(.*)$'))
local M=dofile('core.lua');local final=assert(M.decode(board));local counts=final.status==1 and '65535 0 0'or final.status==2 and '0 65535 0'or'0 0 65535'
local body=counts..'\n'..board;local sum=7;for i=1,#body do sum=(sum*31+body:byte(i))%65521 end
assert(smudge.write_file(path,'UI3:10:'..sum..'\n'..body))
local app=dofile('main.lua');on_init();on_draw();local stats=app.records();assert(stats.wins+stats.losses+stats.draws==65535);capture('long-menu.pgm')
local b={};for _,v in ipairs(app.state().b)do b[#b+1]=v end
-- Exact original model fields, including actual valid saved terminal board.
local write=fixtureWrite
assert(write('long-menu.map',table.concat(b)..'\n'..table.concat({final.turn,final.status,final.selected or 0,final.die or 0,final.idle or 0},' ')..'\n'..(final.history or'')..'\n'..counts..'\n',false))
on_exit();app=dofile('main.lua');on_init();on_draw();assert(app.records().wins+app.records().losses+app.records().draws==65535);on_exit()
