dofile('fixture-gameplay.lua')
local raw=assert(smudge.read_file('native-record-a.dat',1201));local board=assert(raw:match('\n%d+ %d+ %d+\n(.*)$'))
for _,counts in ipairs({'2147483647 1 0','65536 0 0','65535 65535 0'})do
 local body=counts..'\n'..board;local sum=7;for i=1,#body do sum=(sum*31+body:byte(i))%65521 end
 assert(smudge.write_file('native-record-a.dat','UI3:10:'..sum..'\n'..body));local app=dofile('main.lua');on_init();on_draw();local stats=app.records();assert(stats.wins+stats.losses+stats.draws==0,'invalid counters accepted');assert(app.state().status~=0,'malformed optional record harmed game journal');on_exit()
end
